// SPDX-License-Identifier: Apache-2.0
/*
 * Scheduling phase: response-time analysis and optimal priority assignment
 * under precedence constraints (see sched.h).
 */
#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "mapping/sched.h"

/*
 * A lower bound of the response time of a task of WCET @wcet below tasks
 * of total utilization @interference: since ceil(x) >= x, the fixed point
 * R satisfies R >= C + U R, so R >= C / (1 - U) (no fixed point if U >= 1:
 * returns @limit + 1). The small terms keep it below the exact value
 * despite rounding.
 */
static int64_t response_lower_bound(int64_t wcet, double interference,
				    int64_t limit)
{
	double bound;

	if (interference >= 1)
		return limit + 1;
	bound = floor((double)wcet / (1 - interference + 1e-9)) - 1;
	return bound > (double)limit ? limit + 1 : (int64_t)bound;
}

/*
 * Interfering tasks grouped by period: the jobs of tasks of equal period
 * T released in a window of length R are ceil(R / T) each, so they
 * interfere by ceil(R / T) times the sum of their WCETs. A fixed-point
 * iteration then costs one term per distinct period instead of one per
 * task, with exactly the same (integer) iterates.
 */
struct period_groups {
	int count;
	int capacity;
	int64_t *periods;
	int64_t *wcets;		/* sum of the WCETs of the group */
};

static void period_groups_free(struct period_groups *groups)
{
	free(groups->periods);
	free(groups->wcets);
	memset(groups, 0, sizeof(*groups));
}

/* room for @capacity groups, emptied */
static int period_groups_reset(struct period_groups *groups, int capacity)
{
	groups->count = 0;
	if (capacity <= groups->capacity)
		return 0;
	period_groups_free(groups);
	groups->periods = malloc((size_t)capacity * sizeof(*groups->periods));
	groups->wcets = malloc((size_t)capacity * sizeof(*groups->wcets));
	if (!groups->periods || !groups->wcets) {
		period_groups_free(groups);
		return -ENOMEM;
	}
	groups->capacity = capacity;
	return 0;
}

/* add a task (@wcet, @period) to its group; returns the group index */
static int period_groups_add(struct period_groups *groups, int64_t wcet,
			     int64_t period)
{
	int g;

	for (g = 0; g < groups->count; g++)
		if (groups->periods[g] == period) {
			groups->wcets[g] += wcet;
			return g;
		}
	groups->periods[g] = period;
	groups->wcets[g] = wcet;
	groups->count++;
	return g;
}

/*
 * Interference of the groups in a window of length @window, without the
 * task of WCET @self_wcet in group @self (-1: none)
 */
static int64_t group_interference(const struct period_groups *groups,
				  int self, int64_t self_wcet, int64_t window)
{
	int64_t sum = 0;
	int g;

	for (g = 0; g < groups->count; g++) {
		int64_t wcets = groups->wcets[g] - (g == self ? self_wcet : 0);

		sum += (window + groups->periods[g] - 1) / groups->periods[g] *
		       wcets;
	}
	return sum;
}

/*
 * Response time of @task below the higher priority tasks of @groups (the
 * sum of their WCETs @higher_wcets, of their utilizations @interference):
 * the least fixed point of R = C + sum over higher priority tasks j of
 * ceil(R / T_j) C_j. A LET task with no period (empty core) does not
 * interfere. The iteration starts from a lower bound (C plus one job of every higher
 * priority task, or C / (1 - U)) and stops above the deadline; the result
 * is then only known to exceed it (clamped to fit an int).
 */
static int sorted_response_time(const struct mp_task *task,
				int64_t higher_wcets, double interference,
				const struct period_groups *groups)
{
	int64_t response = task->wcet + higher_wcets, previous = -1, bound;

	bound = response_lower_bound(task->wcet, interference, task->period);
	if (bound > response)
		response = bound;
	while (response != previous && response <= task->period) {
		previous = response;
		response = task->wcet + group_interference(groups, -1, 0,
							   previous);
	}
	return response > INT32_MAX ? INT32_MAX : (int)response;
}

int mp_response_times(struct mp_task_array *tasks, bool *schedulable)
{
	struct period_groups groups = {0};
	int64_t higher_wcets = 0;	/* of the tasks above, and their */
	double interference = 0;	/* utilization (in list order) */
	int i, next = 0, err;

	*schedulable = true;
	err = mp_sort_tasks_by_priority(tasks);
	if (!err)
		err = period_groups_reset(&groups, tasks->count + 1);
	if (err)
		return err;

	for (i = 0; i < tasks->count; i++) {
		struct mp_task *task = &tasks->items[i];

		/* the tasks of higher priority (a smaller value) interfere */
		for (; next < i && tasks->items[next].priority < task->priority;
		     next++) {
			const struct mp_task *other = &tasks->items[next];

			if (!other->period)
				continue;
			period_groups_add(&groups, other->wcet, other->period);
			higher_wcets += other->wcet;
			interference += (double)other->wcet / other->period;
		}

		/* LET task of an empty core: no communication to perform */
		if (task->period == 0 && task->wcet == 0) {
			task->response_time = 0;
			continue;
		}
		if (task->wcet > task->period) {
			err = -EINVAL;
			break;
		}

		task->response_time = sorted_response_time(task, higher_wcets,
							   interference,
							   &groups);
		if (task->response_time > task->period)
			*schedulable = false;
	}
	period_groups_free(&groups);
	return err;
}

int mp_core_build_task_list(struct mp_core *core, int core_index)
{
	int chain_slot, task_slot, err;

	mp_task_array_free(&core->tasks);
	for (chain_slot = 0; chain_slot < core->chains.count; chain_slot++) {
		struct mp_chain *chain = &core->chains.items[chain_slot];

		for (task_slot = 0; task_slot < chain->tasks.count; task_slot++) {
			struct mp_task *task = &chain->tasks.items[task_slot];

			task->position.core_index = core_index;
			task->position.chain_slot = chain_slot;
			task->position.task_slot = task_slot;
			err = mp_task_array_push(&core->tasks, task);
			if (err)
				return err;
		}
	}
	return mp_sort_tasks_by_id_in_chain(&core->tasks);
}

void mp_core_write_back_tasks(struct mp_core *core)
{
	int i;

	for (i = 0; i < core->tasks.count; i++) {
		const struct mp_task *task = &core->tasks.items[i];
		struct mp_task *original = &core->chains.items[
			task->position.chain_slot].tasks.items[
			task->position.task_slot];

		original->priority = task->priority;
		original->response_time = task->response_time;
	}
}

/*
 * Priorities in list order: the LET task 1, the other tasks 2, 3, ...
 * @tasks must already be in the wanted order.
 */
static void number_priorities(struct mp_task_array *tasks)
{
	int i, priority = 2;

	for (i = 0; i < tasks->count; i++)
		tasks->items[i].priority = tasks->items[i].is_let ? 1 :
					   priority++;
}

/*
 * Response time of a task (@wcet, @period) below the tasks of @groups,
 * without itself (WCET @wcet in group @self, or -1): least fixed point of
 * R = C + sum ceil(R / T_j) C_j, iterated from @start, a lower bound (any
 * lower bound gives the least fixed point: the right-hand side is
 * monotonic). Stops above @period.
 */
static int64_t response_below(const struct period_groups *groups, int self,
			      int64_t wcet, int64_t period, int64_t start)
{
	int64_t response = start, previous = -1;

	while (response != previous && response <= period) {
		previous = response;
		response = wcet + group_interference(groups, self, wcet,
						     previous);
	}
	return response;
}

/*
 * Audsley's optimal priority assignment (lowest priority first),
 * restricted to the orders that respect precedence: a task may take the
 * lowest free level only when no later task of its activation pattern is
 * left on the core. At each level, every candidate is evaluated with all the remaining
 * tasks above it; among those meeting their deadline, the one with the
 * largest relative slack (T - R) / T gets the level (ties: the latest in
 * precedence order), which leaves the most margin to the higher levels
 * (robust choice, Davis and Burns). Optimal: if some precedence-respecting
 * order is schedulable, this one is (moving a valid bottom task to the
 * bottom keeps any schedulable order schedulable). If no candidate fits a
 * level, the remaining tasks keep the precedence order above the assigned
 * ones and the core is unschedulable.
 *
 * Cost: a first pass takes the first candidate that fits each level
 * (plain Audsley, the same verdict); the robust pass runs only when the
 * core is schedulable. In the robust pass, the sum S of the remaining
 * WCETs is a lower bound of every response time at the level, so
 * (T - S) / T bounds the slack of a candidate; candidates are evaluated by
 * decreasing period and the search stops once the bound cannot beat the
 * best slack found. The bookkeeping between levels is incremental: the
 * remaining tasks stay grouped by period (one term per period in the
 * fixed-point iterations), with the sums S and U updated when a task gets
 * a level, and the eligible tasks are a bit set in candidate order (the
 * task before the chosen one in its pattern joins it). A level then costs
 * the evaluation of its candidates only.
 */
struct audsley {
	struct mp_task_array *tasks;
	bool *assigned;
	int *next;		/* next task of the same pattern on the core */
	int *previous;		/* the task whose next one it is, or -1 */
	int *order;		/* tasks by decreasing period, then index */
	int *position;		/* position of a task in order */
	uint64_t *eligible;	/* by position: may take the next level */
	int *group;		/* period group of a task, or -1 */
	/* the remaining tasks with a period: a task leaves its group when
	 * it gets a level */
	struct period_groups groups;
	int64_t remaining;	/* sum of their WCETs */
	double load;		/* sum of their utilizations */
};

#define BIT_WORD(position)	((position) / 64)
#define BIT_MASK(position)	((uint64_t)1 << ((position) % 64))

/*
 * @order: the task indices by decreasing period, then decreasing index;
 * one pass per distinct period (a core has few)
 */
static void sort_candidate_order(const struct mp_task_array *tasks,
				 int *order)
{
	const struct mp_task *items = tasks->items;
	int n = tasks->count, placed = 0, i;
	int64_t period = INT64_MAX;

	while (placed < n) {
		int64_t next = -1;

		/* the longest period below the previous one */
		for (i = 0; i < n; i++)
			if (items[i].period < period && items[i].period > next)
				next = items[i].period;
		for (i = n - 1; i >= 0; i--)
			if (items[i].period == next)
				order[placed++] = i;
		period = next;
	}
}

/*
 * The task that takes the lowest free level: the first candidate that
 * meets its deadline, or (@robust) the one with the largest relative
 * slack. -1 if none does. The candidates are the eligible tasks, by
 * decreasing period, then decreasing index.
 */
static int choose_for_level(struct audsley *a, bool robust)
{
	int n = a->tasks->count, words = (n + 63) / 64, chosen = -1, w;
	double best_slack = -1;

	for (w = 0; w < words; w++) {
		uint64_t bits = a->eligible[w];

		while (bits) {
			int index = a->order[w * 64 + __builtin_ctzll(bits)];
			const struct mp_task *task = &a->tasks->items[index];
			int64_t response, start;
			double slack;

			bits &= bits - 1;
			if ((double)(task->period - a->remaining) / task->period <
			    best_slack)
				return chosen;	/* no later one can do better */
			start = response_lower_bound(task->wcet, a->load -
						     (double)task->wcet /
						     task->period, task->period);
			if (start < a->remaining)
				start = a->remaining;
			if ((double)(task->period - start) / task->period <
			    best_slack)
				continue;	/* cannot beat the best either */
			response = response_below(&a->groups, a->group[index],
						  task->wcet, task->period,
						  start);
			if (response > task->period)
				continue;
			if (!robust)
				return index;
			slack = (double)(task->period - response) / task->period;
			if (slack > best_slack ||
			    (slack == best_slack && index > chosen)) {
				best_slack = slack;
				chosen = index;
			}
		}
	}
	return chosen;
}

/*
 * next[i]: the task that follows task i in its activation pattern on the
 * core (same chain, same period). Tasks of one pattern are released
 * together and run in chain order, so their priorities must decrease along
 * the chain; tasks of different patterns communicate through registers
 * and have no precedence.
 */
struct pattern_key {
	int chain_id;
	int period;
	int id_in_chain;
	int index;
};

static int pattern_key_compare(const void *a, const void *b)
{
	const struct pattern_key *first = a, *second = b;

	if (first->chain_id != second->chain_id)
		return first->chain_id < second->chain_id ? -1 : 1;
	if (first->period != second->period)
		return first->period < second->period ? -1 : 1;
	if (first->id_in_chain != second->id_in_chain)
		return first->id_in_chain < second->id_in_chain ? -1 : 1;
	return first->index < second->index ? -1 : first->index > second->index;
}

/* the tasks sorted by pattern (chain, period), then position in it */
static int link_chain_successors(struct audsley *a)
{
	const struct mp_task *items = a->tasks->items;
	int n = a->tasks->count, count = 0, i;
	struct pattern_key *keys = malloc(((size_t)n + 1) * sizeof(*keys));

	if (!keys)
		return -ENOMEM;
	for (i = 0; i < n; i++) {
		a->next[i] = -1;
		a->previous[i] = -1;
		if (items[i].is_let)
			continue;
		keys[count].chain_id = items[i].chain_id;
		keys[count].period = items[i].period;
		keys[count].id_in_chain = items[i].id_in_chain;
		keys[count].index = i;
		count++;
	}
	qsort(keys, (size_t)count, sizeof(*keys), pattern_key_compare);
	for (i = 0; i + 1 < count; i++) {
		const struct pattern_key *key = &keys[i], *after = &keys[i + 1];

		if (after->chain_id != key->chain_id ||
		    after->period != key->period ||
		    after->id_in_chain <= key->id_in_chain)
			continue;
		a->next[key->index] = after->index;
		a->previous[after->index] = key->index;
	}
	free(keys);
	return 0;
}

/* every level from the lowest; false if one could not be filled */
static bool assign_levels(struct audsley *a, bool robust)
{
	const struct mp_task *items = a->tasks->items;
	int n = a->tasks->count, words = (n + 63) / 64, level, chosen, i;

	memset(a->assigned, 0, ((size_t)n + 1) * sizeof(*a->assigned));
	memset(a->eligible, 0, ((size_t)words + 1) * sizeof(*a->eligible));
	a->groups.count = 0;
	a->remaining = 0;
	a->load = 0;
	for (i = 0; i < n; i++) {
		a->group[i] = -1;
		if (items[i].period) {
			a->group[i] = period_groups_add(&a->groups,
							items[i].wcet,
							items[i].period);
			a->remaining += items[i].wcet;
			a->load += (double)items[i].wcet / items[i].period;
		}
		/* the last task of its pattern may take the lowest level */
		if (!items[i].is_let && a->next[i] < 0)
			a->eligible[BIT_WORD(a->position[i])] |=
				BIT_MASK(a->position[i]);
	}
	/* the LET task keeps priority 1 */
	for (level = n; level > 1; level--) {
		int before;

		chosen = choose_for_level(a, robust);
		if (chosen < 0)
			return false;
		a->assigned[chosen] = true;
		a->tasks->items[chosen].priority = level;
		a->eligible[BIT_WORD(a->position[chosen])] &=
			~BIT_MASK(a->position[chosen]);
		if (a->group[chosen] >= 0) {
			a->groups.wcets[a->group[chosen]] -= items[chosen].wcet;
			a->remaining -= items[chosen].wcet;
			a->load -= (double)items[chosen].wcet /
				   items[chosen].period;
		}
		/* the task before it in its pattern becomes eligible */
		before = a->previous[chosen];
		if (before >= 0)
			a->eligible[BIT_WORD(a->position[before])] |=
				BIT_MASK(a->position[before]);
	}
	return true;
}

static int audsley_priorities(struct mp_ctx *ctx, struct mp_core *core,
			      bool robust)
{
	struct mp_task_array *tasks = &core->tasks;
	size_t size = (size_t)tasks->count + 1;
	struct audsley a = {.tasks = tasks};
	bool schedulable;
	int i, j, err;

	err = mp_sort_tasks_by_id_in_chain(tasks);
	if (err)
		return err;
	number_priorities(tasks);	/* precedence order, the fallback */
	a.assigned = calloc(size, sizeof(*a.assigned));
	a.next = malloc(size * sizeof(*a.next));
	a.previous = malloc(size * sizeof(*a.previous));
	a.order = malloc(size * sizeof(*a.order));
	a.position = malloc(size * sizeof(*a.position));
	a.eligible = malloc((size / 64 + 2) * sizeof(*a.eligible));
	a.group = malloc(size * sizeof(*a.group));
	if (!a.assigned || !a.next || !a.previous || !a.order ||
	    !a.position || !a.eligible || !a.group ||
	    period_groups_reset(&a.groups, (int)size)) {
		err = -ENOMEM;
		goto out;
	}
	err = link_chain_successors(&a);
	if (err)
		goto out;
	sort_candidate_order(tasks, a.order);
	for (i = 0; i < tasks->count; i++)
		a.position[a.order[i]] = i;
	if (assign_levels(&a, false) && robust)
		assign_levels(&a, true);

	/*
	 * the LET task first; tasks left without a level (no candidate fitted
	 * level L) take 2..L in base order, above the assigned ones
	 */
	for (i = 0, j = 2; i < tasks->count; i++)
		if (tasks->items[i].is_let)
			tasks->items[i].priority = 1;
		else if (!a.assigned[i])
			tasks->items[i].priority = j++;

	err = mp_response_times(tasks, &schedulable);
	if (err)
		goto out;
	core->schedulable = schedulable;
	mp_core_write_back_tasks(core);
	if (schedulable)
		ctx->priority_successes++;
out:
	free(a.assigned);
	free(a.next);
	free(a.previous);
	free(a.order);
	free(a.position);
	free(a.eligible);
	free(a.group);
	period_groups_free(&a.groups);
	return err;
}

int mp_assign_priorities(struct mp_ctx *ctx, struct mp_core *core)
{
	ctx->priority_assignments++;
	return audsley_priorities(ctx, core, true);
}

int mp_assign_priorities_first_fit(struct mp_ctx *ctx, struct mp_core *core)
{
	ctx->priority_assignments++;
	return audsley_priorities(ctx, core, false);
}

int mp_schedule(struct mp_ctx *ctx, struct mp_core_array *cores)
{
	int i, err;

	err = mp_sort_cores_by_increasing_room(cores);
	if (err)
		return err;
	for (i = 0; i < cores->count; i++) {
		err = mp_core_build_task_list(&cores->items[i], i);
		if (err)
			return err;
	}
	for (i = 0; i < cores->count; i++) {
		if (!cores->items[i].load)
			continue;	/* nothing to schedule */
		err = mp_assign_priorities(ctx, &cores->items[i]);
		if (err)
			return err;
	}
	mp_count_schedulable_cores(ctx, cores);
	ctx->rate_after_scheduling = ctx->schedulable_rate;
	return 0;
}

void mp_count_schedulable_cores(struct mp_ctx *ctx,
				const struct mp_core_array *cores)
{
	int i;

	ctx->schedulable_cores = 0;
	ctx->unschedulable_cores = 0;
	for (i = 0; i < cores->count; i++) {
		if (cores->items[i].schedulable)
			ctx->schedulable_cores++;
		else
			ctx->unschedulable_cores++;
	}
	/* NaN without cores */
	ctx->schedulable_rate = (float)ctx->schedulable_cores /
		(float)(ctx->schedulable_cores + ctx->unschedulable_cores);
}

/*
 * Whether the tasks of @core meet their deadlines with WCETs x @factor.
 * Times are converted to nanoseconds first, so that scaling a WCET of a
 * few microseconds is not dominated by rounding (periods up to 1 s stay
 * below 2^31 ns).
 */
static bool schedulable_scaled(const struct mp_core *core,
			       struct mp_task_array *scaled, double factor)
{
	bool schedulable;
	int i;

	for (i = 0; i < core->tasks.count; i++) {
		const struct mp_task *task = &core->tasks.items[i];
		struct mp_task *copy = &scaled->items[i];

		*copy = *task;
		copy->period = task->period * 1000;
		copy->wcet = task->is_let ? task->wcet * 1000 :
			     (int)ceil(task->wcet * factor * 1000);
		if (copy->wcet > copy->period)
			return false;
	}
	if (mp_response_times(scaled, &schedulable))
		return false;
	return schedulable;
}

double mp_core_scaling_factor(const struct mp_core *core)
{
	struct mp_task_array scaled = {0};
	double low = 0, high = 1;
	int i;

	if (!core->load || !core->tasks.count)
		return -1;
	for (i = 0; i < core->tasks.count; i++)
		if (mp_task_array_push(&scaled, &core->tasks.items[i]))
			goto out;

	/* bracket: double the factor while the core stays schedulable */
	while (high < MP_MAX_SCALING_FACTOR &&
	       schedulable_scaled(core, &scaled, high)) {
		low = high;
		high *= 2;
	}
	if (high >= MP_MAX_SCALING_FACTOR &&
	    schedulable_scaled(core, &scaled, MP_MAX_SCALING_FACTOR)) {
		low = MP_MAX_SCALING_FACTOR;
		goto out;
	}
	/* then bisection to 0.1 % */
	while (high - low > 1e-3 * high) {
		double middle = (low + high) / 2;

		if (schedulable_scaled(core, &scaled, middle))
			low = middle;
		else
			high = middle;
	}
out:
	mp_task_array_free(&scaled);
	return low;
}
