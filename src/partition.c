// SPDX-License-Identifier: Apache-2.0
/* Partitioning step (see partition.h). */
#include <errno.h>
#include <string.h>

#include "mapping/partition.h"
#include "mapping/sched.h"

/* the fragment being built while walking through one chain */
struct open_fragment {
	struct mp_task_array tasks;
	int utilization;	/* sum of the task utilizations, ppm */
	int controller;		/* controller of its bound tasks, or ANY */
	unsigned allowed;	/* controllers its tasks may run on */
	unsigned types;		/* core types its tasks may run on */
	int memory;		/* sum of the task memories, KB */
	int next_index;		/* fragment index the fragment will get */
};

/*
 * Close the open fragment of @source (if it has tasks): append it to
 * @fragments and start an empty one.
 */
static int close_fragment(const struct mp_chain *source,
			  struct open_fragment *open,
			  struct mp_chain_array *fragments)
{
	struct mp_chain fragment;
	int err;

	if (!open->tasks.count)
		return 0;

	memset(&fragment, 0, sizeof(fragment));
	fragment.id = source->id;
	fragment.fragment_index = open->next_index;
	fragment.utilization = open->utilization;
	fragment.controller = open->controller;
	fragment.allowed = open->allowed;
	fragment.memory = open->memory;
	fragment.replica_group = source->replica_group;
	fragment.deadline = source->deadline;
	fragment.is_fragment = true;
	fragment.tasks = open->tasks;	/* the fragment takes the tasks */

	err = mp_chain_array_push(fragments, &fragment);
	mp_task_array_free(&open->tasks);
	if (err)
		return err;
	open->next_index++;
	open->utilization = 0;
	open->memory = 0;
	open->controller = MP_CONTROLLER_ANY;
	return 0;
}

/*
 * Whether the tasks of a fragment are schedulable alone, with the
 * priorities of chain order inside an activation pattern and rate
 * monotonic across patterns (shorter period first); @tasks keeps its
 * order.
 */
static int fragment_schedulable(const struct mp_task_array *tasks,
				bool *schedulable)
{
	struct mp_task_array copy = {0};
	int i, j, err;

	err = mp_task_array_copy(&copy, tasks);
	if (err)
		return err;
	for (i = 0; i < copy.count; i++) {
		int rank = 1;

		for (j = 0; j < copy.count; j++)
			if (copy.items[j].period < copy.items[i].period ||
			    (copy.items[j].period == copy.items[i].period &&
			     j < i))
				rank++;
		copy.items[i].priority = rank;
	}
	err = mp_response_times(&copy, schedulable);
	mp_task_array_free(&copy);
	return err;
}

/*
 * Cut one chain into fragments appended to @fragments; counts the cuts
 * made and, among them, the cuts forced by a change of binding (free
 * task, or task bound to another controller).
 */
/*
 * The size limit of a fragment of tasks allowed on @allowed, in ppm of
 * the reference core: sigma (minus the LET margin) of the core it will
 * run on: a central computer if it may, else the first ECU it may run on,
 * of the first type of that ECU allowed (a slower core takes a smaller
 * part of the reference one).
 */
static int limit_for(const struct mp_params *params, unsigned allowed,
		     unsigned types)
{
	int limit = params->sigma - MP_LET_MARGIN, speed = 0, pass, c, t;

	for (pass = 0; pass < 2 && !speed; pass++)
		for (c = 0; c < mp_ecu_count(params) && !speed; c++) {
			unsigned usable = types & mp_ecu_types(params, c);

			if (!(allowed & MP_CONTROLLER_BIT(c)) || !usable ||
			    mp_ecu_is_host(params, c) != !pass)
				continue;
			for (t = 0; !(usable & (1u << t)); t++)
				;
			speed = mp_ecu_speed(params, c, t);
		}
	if (!speed)	/* runs nowhere: the assignment will say so */
		speed = MP_REFERENCE_SPEED;
	return (int)((int64_t)limit * speed / MP_REFERENCE_SPEED);
}

static int partition_chain(const struct mp_params *params,
			   const struct mp_chain *chain,
			   struct mp_chain_array *fragments, int *cuts,
			   int *forced_cuts)
{
	struct open_fragment open;
	int first_fragment = fragments->count;
	int size_limit, j, err = 0;

	memset(&open, 0, sizeof(open));
	open.controller = MP_CONTROLLER_ANY;

	for (j = 0; j < chain->tasks.count; j++) {
		struct mp_task task = chain->tasks.items[j];
		bool schedulable, glued;

		task.chain_id = chain->id;

		/*
		 * the tasks of a fragment share their binding and the
		 * controllers they may run on: all free, or all bound to the
		 * same controller (a sensor or actuator driver does not drag
		 * the computation into its zone)
		 */
		/* co-located with the previous task: no cut in between */
		glued = open.tasks.count &&
			open.tasks.items[open.tasks.count - 1].colocate_next;
		if (open.tasks.count && (task.controller != open.controller ||
					 mp_task_allowed(&task) != open.allowed ||
					 mp_task_types(&task) != open.types)) {
			/* mp_chain_colocate() gives co-located tasks the
			 * same constraints */
			if (glued) {
				err = -EINVAL;
				goto out;
			}
			err = close_fragment(chain, &open, fragments);
			if (err)
				goto out;
			(*forced_cuts)++;
		}

		size_limit = limit_for(params, mp_task_allowed(&task),
				       mp_task_types(&task));

		/* a task alone above the limit becomes its own fragment */
		if (task.utilization > size_limit && !glued) {
			err = close_fragment(chain, &open, fragments);
			if (!err)
				err = mp_task_array_push(&open.tasks, &task);
			if (err)
				goto out;
			open.utilization = task.utilization;
			open.memory = task.memory;
			open.controller = task.controller;
			open.allowed = mp_task_allowed(&task);
			open.types = mp_task_types(&task);
			if (!task.colocate_next) {
				err = close_fragment(chain, &open, fragments);
				if (err)
					goto out;
			}
			continue;
		}

		err = mp_task_array_push(&open.tasks, &task);
		if (err)
			goto out;
		open.utilization += task.utilization;
		open.memory += task.memory;
		open.controller = task.controller;
		open.allowed = mp_task_allowed(&task);
		open.types = mp_task_types(&task);
		if (glued)
			continue;

		/* the fragment must be schedulable alone */
		err = fragment_schedulable(&open.tasks, &schedulable);
		if (err)
			goto out;

		/* if adding the task breaks a constraint, the fragment is
		 * closed without it and the task starts the next one */
		if (open.tasks.count > 1 &&
		    (!schedulable || open.utilization > size_limit)) {
			open.utilization -= task.utilization;
			open.memory -= task.memory;
			open.tasks.count--;
			err = close_fragment(chain, &open, fragments);
			if (err)
				goto out;
			j--;
		}
	}
	err = close_fragment(chain, &open, fragments);
	if (!err && fragments->count > first_fragment)
		*cuts += fragments->count - first_fragment - 1;
out:
	mp_task_array_free(&open.tasks);
	return err;
}

int mp_partition(struct mp_ctx *ctx, struct mp_chain_array *chains)
{
	struct mp_chain_array fragments;
	int i, err;

	/* every edge of every chain cut */
	ctx->max_cuts = 0;
	for (i = 0; i < chains->count; i++)
		if (!chains->items[i].is_let)
			ctx->max_cuts += chains->items[i].tasks.count - 1;

	ctx->cuts = 0;
	ctx->forced_cuts = 0;
	memset(&fragments, 0, sizeof(fragments));
	for (i = 0; i < chains->count; i++) {
		err = partition_chain(&ctx->params, &chains->items[i], &fragments,
				      &ctx->cuts, &ctx->forced_cuts);
		if (err)
			goto err_free_fragments;
	}

	err = mp_sort_chains_by_decreasing_utilization(&fragments);
	if (err)
		goto err_free_fragments;
	for (i = 0; i < fragments.count; i++)
		fragments.items[i].period_gcd =
			mp_tasks_period_gcd(&fragments.items[i].tasks);

	mp_chain_array_free(chains);
	*chains = fragments;
	return 0;

err_free_fragments:
	mp_chain_array_free(&fragments);
	return err;
}
