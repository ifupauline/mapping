// SPDX-License-Identifier: Apache-2.0
/*
 * Invariant checker: runs the whole methodology on many random
 * applications and checks the properties the model requires, with
 * independent recomputations (exact WCRT, WCETs on the core speeds,
 * latencies and end-to-end delays).
 *
 * Usage: check_invariants [runs]	(default 20; each run = 5 sigmas x 3
 *					 algorithms, under varied parameters)
 */
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mapping/assign.h"
#include "mapping/communication.h"
#include "mapping/generator.h"
#include "mapping/let.h"
#include "mapping/sched.h"
#include "mapping/network.h"
#include "mapping/pipeline.h"

#define MAX_KINDS 64

/* failures by kind, with the first example of each */
static struct {
	const char *what;
	int count;
	char example[160];
} failures[MAX_KINDS];
static int failure_kinds;
static int incremental_moves;	/* moves checked by check_incremental() */

static void fail(const char *what, const char *format, ...)
	__attribute__((format(printf, 2, 3)));

static void fail(const char *what, const char *format, ...)
{
	va_list args;
	int i;

	for (i = 0; i < failure_kinds; i++)
		if (!strcmp(failures[i].what, what))
			break;
	if (i == failure_kinds) {
		if (failure_kinds == MAX_KINDS)
			return;
		failure_kinds++;
		failures[i].what = what;
		va_start(args, format);
		vsnprintf(failures[i].example, sizeof(failures[i].example),
			  format, args);
		va_end(args);
	}
	failures[i].count++;
}

/*
 * exact WCRT of @task among @tasks (the fixed point iterated from C),
 * stopped above the period
 */
static long exact_response_time(const struct mp_task_array *tasks,
				const struct mp_task *task)
{
	long response = task->wcet, previous = -1;
	int j;

	while (response != previous && response <= task->period) {
		previous = response;
		response = task->wcet;
		for (j = 0; j < tasks->count; j++) {
			const struct mp_task *other = &tasks->items[j];

			if (other->priority < task->priority && other->period > 0)
				response += ((previous + other->period - 1) /
					     other->period) * other->wcet;
		}
	}
	return response;
}

/* partitioning keeps every task once, in contiguous fragments */
static void check_partition(const struct mp_chain_array *application,
			    const struct mp_chain_array *fragments, int sigma)
{
	int before = 0, after = 0, i, k;

	for (i = 0; i < application->count; i++)
		before += application->items[i].tasks.count;
	for (i = 0; i < fragments->count; i++) {
		const struct mp_chain *fragment = &fragments->items[i];
		int sum = 0;
		bool glued;

		after += fragment->tasks.count;
		for (k = 0; k < fragment->tasks.count; k++) {
			const struct mp_task *task = &fragment->tasks.items[k];

			sum += task->utilization;
			if (task->controller != fragment->controller)
				fail("partition: tasks of a fragment with another binding",
				     "chain %d index %d", fragment->id,
				     fragment->fragment_index);
			if (task->chain_id != fragment->id)
				fail("partition: task chain id != fragment id",
				     "chain %d", fragment->id);
			if (k && task->id_in_chain !=
			    fragment->tasks.items[k - 1].id_in_chain + 1)
				fail("partition: fragment not contiguous",
				     "chain %d index %d", fragment->id,
				     fragment->fragment_index);
		}
		if (sum != fragment->utilization)
			fail("partition: fragment size != sum of its tasks",
			     "chain %d u %d actual %d", fragment->id,
			     fragment->utilization, sum);
		/* co-located tasks may take a fragment above sigma */
		glued = false;
		for (k = 0; k + 1 < fragment->tasks.count; k++)
			glued |= fragment->tasks.items[k].colocate_next;
		if (fragment->tasks.count > 1 && sum > sigma && !glued)
			fail("partition: multi-task fragment above sigma",
			     "chain %d u %d sigma %d", fragment->id, sum, sigma);
	}
	if (before != after)
		fail("partition: tasks lost or duplicated", "%d before, %d after",
		     before, after);
}

static void check_cores(const struct mp_core_array *cores, int fragment_count)
{
	int placed = 0, i, j, k, l;

	for (i = 0; i < cores->count; i++) {
		const struct mp_core *core = &cores->items[i];
		struct mp_task_array all = {0};
		int let_tasks = 0, load = 0, application_tasks = 0;
		bool exact_ok = true;

		for (j = 0; j < core->chains.count; j++) {
			const struct mp_chain *chain = &core->chains.items[j];

			load += chain->utilization;
			if (!chain->is_let) {
				placed++;
				application_tasks += chain->tasks.count;
				if (chain->controller != MP_CONTROLLER_ANY &&
				    chain->controller != core->controller)
					fail("assign: bound fragment on another controller",
					     "chain %d", chain->id);
			}
			for (k = 0; k < chain->tasks.count; k++) {
				const struct mp_task *task = &chain->tasks.items[k];

				mp_task_array_push(&all, task);
				if (!chain->is_let) {
					if (task->wcet != mp_task_wcet_on_core(task,
							core) ||
					    task->utilization !=
					    mp_utilization(task->wcet,
							   task->period))
						fail("speed: WCET or utilization not those of the core",
						     "core %d chain %d task %d",
						     core->id, chain->id,
						     task->id_in_chain);
					continue;
				}
				let_tasks++;
				if (!task->is_let || task->priority != 1)
					fail("LET task lost its identity or priority 1",
					     "core %d", core->id);
			}
		}
		if (let_tasks != 1)
			fail("core without exactly one LET task", "core %d has %d",
			     core->id, let_tasks);
		if (load > core->fill_limit)
			fail("core loaded beyond phi", "core %d load %d",
			     core->id, load);
		if (!application_tasks && load)
			fail("empty core still carries LET load", "core %d load %d",
			     core->id, load);

		/* precedence: in one pattern, priority follows the task order */
		for (k = 0; k < all.count; k++)
			for (l = 0; l < all.count; l++) {
				const struct mp_task *a = &all.items[k];
				const struct mp_task *b = &all.items[l];

				if (!a->is_let && !b->is_let &&
				    a->chain_id == b->chain_id &&
				    a->period == b->period &&
				    a->id_in_chain < b->id_in_chain &&
				    a->priority >= b->priority)
					fail("precedence violated (successor first)",
					     "core %d chain %d", core->id,
					     a->chain_id);
			}

		/* the flag and the stored response times agree with the RTA */
		for (k = 0; k < all.count && application_tasks; k++) {
			const struct mp_task *task = &all.items[k];
			long exact;

			if (task->is_let || !task->period)
				continue;
			exact = exact_response_time(&all, task);
			if (exact > task->period)
				exact_ok = false;
			if ((exact <= task->period) !=
			    (task->response_time <= task->period))
				fail("stored response time disagrees with exact WCRT",
				     "core %d task %d stored %d exact %ld", core->id,
				     task->id_in_chain, task->response_time, exact);
		}
		if (application_tasks && core->schedulable != exact_ok)
			fail("core flag disagrees with the exact WCRT test",
			     "core %d", core->id);
		mp_task_array_free(&all);
	}
	if (placed != fragment_count)
		fail("assign: not every fragment placed exactly once",
		     "%d placed of %d", placed, fragment_count);
}

/* where task @task of chain @chain_id runs: core id, or -1 */
static int core_of_task(const struct mp_core_array *cores, int chain_id,
			int task)
{
	int i, j, k;

	for (i = 0; i < cores->count; i++)
		for (j = 0; j < cores->items[i].chains.count; j++) {
			const struct mp_chain *chain = &cores->items[i].chains.items[j];

			if (chain->is_let || chain->id != chain_id)
				continue;
			for (k = 0; k < chain->tasks.count; k++)
				if (chain->tasks.items[k].id_in_chain == task)
					return cores->items[i].id;
		}
	return -1;
}

/*
 * spatial constraints: allowed controllers, memory of the controllers,
 * replicas apart, co-located tasks on one core
 */
static void check_constraints(const struct mp_params *params,
			      const struct mp_core_array *cores)
{
	int memory[MP_MAX_ECUS] = {0}, c, i, j, k, i2, j2;

	for (i = 0; i < cores->count; i++) {
		const struct mp_core *core = &cores->items[i];

		for (j = 0; j < core->chains.count; j++) {
			const struct mp_chain *chain = &core->chains.items[j];

			if (chain->is_let)
				continue;
			for (k = 0; k < chain->tasks.count; k++) {
				const struct mp_task *task = &chain->tasks.items[k];

				memory[core->controller] += task->memory;
				if (!(mp_task_types(task) & (1u << core->type)))
					fail("constraint: task on a forbidden core type",
					     "chain %d task %d", chain->id,
					     task->id_in_chain);
				if (!(mp_task_allowed(task) &
				      MP_CONTROLLER_BIT(core->controller)))
					fail("constraint: task on a forbidden controller",
					     "chain %d task %d", chain->id,
					     task->id_in_chain);
				if (task->colocate_next &&
				    core_of_task(cores, chain->id,
						 task->id_in_chain + 1) != core->id)
					fail("constraint: co-located tasks apart",
					     "chain %d task %d", chain->id,
					     task->id_in_chain);
			}
			if (!chain->replica_group)
				continue;
			/* no other chain of its group on the same controller or
			 * core */
			for (i2 = 0; i2 < cores->count; i2++) {
				const struct mp_core *other = &cores->items[i2];

				if (params->separation == MP_SEPARATE_CORES ?
				    i2 != i : other->controller != core->controller)
					continue;
				for (j2 = 0; j2 < other->chains.count; j2++)
					if (!other->chains.items[j2].is_let &&
					    other->chains.items[j2].replica_group ==
					    chain->replica_group &&
					    other->chains.items[j2].id != chain->id)
						fail("constraint: replicas together",
						     "chains %d and %d", chain->id,
						     other->chains.items[j2].id);
			}
		}
	}
	for (c = 0; c < mp_ecu_count(params); c++) {
		int capacity = mp_ecu_memory(params, c);

		if (capacity > 0 && memory[c] > capacity)
			fail("constraint: controller memory exceeded",
			     "controller %d: %d KB of %d", c, memory[c], capacity);
	}
}

/* where a task of a chain runs, as found on the cores */
struct placed_task {
	int core_id;
	int response_time;
	int period;
	int let_period;		/* LET task of its core */
	int let_wcet;
	bool late;
};

/* the tasks of chain @chain_id in chain order; returns their number */
static int place_tasks(const struct mp_core_array *cores, int chain_id,
		       struct placed_task *placed, int capacity)
{
	int count = 0, i, j, t;

	for (i = 0; i < cores->count; i++) {
		const struct mp_core *core = &cores->items[i];
		int let_period = 0, let_wcet = 0;

		for (j = 0; j < core->chains.count; j++)
			if (core->chains.items[j].is_let) {
				let_period = core->chains.items[j].tasks.items[0].period;
				let_wcet = core->chains.items[j].tasks.items[0].wcet;
			}
		for (j = 0; j < core->chains.count; j++) {
			const struct mp_chain *chain = &core->chains.items[j];

			if (chain->is_let || chain->id != chain_id)
				continue;
			for (t = 0; t < chain->tasks.count; t++) {
				const struct mp_task *task = &chain->tasks.items[t];
				int k = task->id_in_chain;

				if (k < 0 || k >= capacity)
					return -1;
				placed[k].core_id = core->id;
				placed[k].response_time = task->response_time;
				placed[k].period = task->period;
				placed[k].let_period = let_period;
				placed[k].let_wcet = let_wcet;
				placed[k].late = task->response_time > task->period;
				count++;
			}
		}
	}
	return count;
}

/*
 * Paths, latencies and end-to-end delays against a recomputation task by task: a
 * run of consecutive tasks on one core with one period costs the WCRT of
 * its last task; a new run on the same core waits for a register read
 * (its period), on another core for the LET write and read; every
 * inter-controller hop adds its network bound.
 */
static void check_communication(const struct mp_design_point *point,
				int chain_count)
{
	struct placed_task placed[MP_GEN_MAX_TASKS * 4];
	const struct mp_core_array *cores = &point->cores;
	int hops = 0, p, k, i;

	if (point->paths.count != chain_count)
		fail("comm: not one path per original chain", "%d paths, %d chains",
		     point->paths.count, chain_count);
	for (p = 0; p < point->paths.count; p++) {
		const struct mp_chain_path *path = &point->paths.items[p];
		int latency = 0, age = 0, count, fragments = 0, network = 0;
		bool schedulable = true;

		for (i = 0; i < point->fragments.count; i++)
			fragments += point->fragments.items[i].id == path->chain_id;
		if (fragments != path->fragment_count)
			fail("comm: fragment count differs from partitioning",
			     "chain %d", path->chain_id);
		if (path->hop_count + 1 != path->segment_count)
			fail("comm: hops != segments - 1", "chain %d", path->chain_id);
		hops += path->hop_count;

		count = place_tasks(cores, path->chain_id, placed,
				    MP_GEN_MAX_TASKS * 4);
		for (k = 0; k < count; k++) {
			schedulable &= !placed[k].late;
			if (k + 1 < count &&
			    placed[k + 1].core_id == placed[k].core_id &&
			    placed[k + 1].period == placed[k].period)
				continue;	/* the run goes on */
			latency += placed[k].response_time;
			if (k + 1 == count)
				break;
			if (placed[k + 1].core_id == placed[k].core_id)
				age += placed[k + 1].period;	/* register */
			else if (point->ctx.params.network.strict_let) {
				/* strict LET: published at the period end */
				int wait = placed[k].period + placed[k].let_wcet -
					   placed[k].response_time;

				age += (wait > placed[k].let_wcet ? wait :
					placed[k].let_wcet) + placed[k + 1].period;
			} else
				age += placed[k].let_period + placed[k].let_wcet +
				       placed[k + 1].period;	/* implicit */
		}

		for (k = 0; k < path->hop_count; k++) {
			const struct mp_hop *hop = &path->hops[k];
			bool same = hop->flow.source_controller ==
				    hop->flow.destination_controller;

			if (hop->source_core == hop->destination_core)
				fail("comm: hop inside one core", "chain %d",
				     path->chain_id);
			if (same != (hop->type == MP_HOP_INTRA_ECU))
				fail("comm: wrong hop type", "chain %d",
				     path->chain_id);
			if (hop->type != MP_HOP_INTER_ECU || !hop->flow.network_ok)
				continue;
			network += (int)ceil(hop->flow.total_delay);
			if (!(hop->flow.total_delay > MP_NET_SWITCH_LATENCY))
				fail("net: bound not above the switch latency",
				     "chain %d", path->chain_id);
			if (hop->flow.period <= 0 || hop->flow.message_bytes <= 0)
				fail("net: bad message parameters", "chain %d",
				     path->chain_id);
		}
		latency += network;
		age += latency;
		if (latency != path->latency)
			fail("comm: latency differs from the recomputation",
			     "chain %d: %d vs %d", path->chain_id, path->latency,
			     latency);
		if (age != path->e2e_delay)
			fail("comm: end-to-end delay differs from the recomputation",
			     "chain %d: %d vs %d", path->chain_id, path->e2e_delay,
			     age);
		if (path->reaction_time != path->e2e_delay + placed[0].period)
			fail("comm: reaction time != end-to-end delay + first period",
			     "chain %d", path->chain_id);
		if (path->meets_deadline !=
		    (!path->deadline || path->reaction_time <= path->deadline) ||
		    path->on_time != (path->schedulable && path->network_ok &&
				      path->meets_deadline))
			fail("comm: deadline or on-time flag wrong", "chain %d",
			     path->chain_id);
		if (schedulable != path->schedulable)
			fail("comm: chain schedulability wrong", "chain %d",
			     path->chain_id);
	}
	if (hops != point->communication.intra_ecu_hops +
		    point->communication.inter_ecu_hops)
		fail("comm: hop counters != number of hops", "%d hops", hops);
}

/*
 * the parameters of run @run: the defaults, then in turn slower or faster
 * zone controllers, a WCET margin, frame preemption, no affinity, a full
 * fill limit, shared-memory contention, and other networks (ring with
 * three classes and frame replication, tree with time-aware shaping, a
 * 10 Gbit/s backbone with two classes)
 */
static void vary_parameters(struct mp_params *params,
			    struct mp_gen_params *generator, int run)
{
	/* constraints in every other run */
	if (run % 2) {
		generator->replica_percent = 10 + 5 * (run % 3);
		generator->colocate_percent = 20;
		generator->zone_only_percent = 10;
		params->zcu_memory = 32768;
		if (run % 4 == 3)
			params->separation = MP_SEPARATE_CORES;
	}
	/* heterogeneous cores in every third run */
	if (run % 3 == 2) {
		params->pcu_cores = 4;
		params->pcu_little_cores = 4;
		params->zcu_cores = 2;
		params->zcu_lockstep_cores = 2;
		generator->type_variation = 40;
		generator->lockstep_percent = 10;
	}
	/* sizing for a margin in every fifth run */
	if (run % 5 == 4)
		params->target_scaling = 1300;
	/* strict LET between cores in every fourth run */
	if (run % 4 == 1)
		params->network.strict_let = true;
	/* shared-memory contention in every seventh run */
	if (run % 7 == 3)
		params->memory_latency = 50;
	switch (run % 9) {
	case 6:
		params->network.topology = MP_NET_RING;
		mp_net_set_classes(&params->network, 3);
		/* frame replication for the replicated chains */
		params->network.frer = run % 2;
		break;
	case 7:
		params->network.topology = MP_NET_TREE;
		params->network.shaping = MP_NET_TAS;
		break;
	case 8:
		params->network.backbone_mbps = 10000;
		mp_net_set_classes(&params->network, 2);
		break;
	case 1:
		params->zcu_speed = 25;
		break;
	case 2:
		params->wcet_margin = 20;
		params->zcu_speed = 100;
		break;
	case 3:
		params->network.preemption = true;
		params->let_wcet = 200;
		break;
	case 4:
		params->affinity = false;
		params->repair_deadlines = false;
		break;
	case 5:
		params->fill_limit = MP_UTILIZATION_UNIT;
		break;
	default:
		break;
	}
}

/* whether two analyses of the same mapping agree on every result */
static bool same_analysis(const struct mp_path_array *a,
			  const struct mp_communication_summary *sa,
			  const struct mp_path_array *b,
			  const struct mp_communication_summary *sb)
{
	int i, j;

	if (a->count != b->count ||
	    sa->intra_ecu_hops != sb->intra_ecu_hops ||
	    sa->inter_ecu_hops != sb->inter_ecu_hops ||
	    sa->on_time_chains != sb->on_time_chains ||
	    sa->deadline_misses != sb->deadline_misses ||
	    sa->schedulable_chains != sb->schedulable_chains ||
	    sa->max_e2e_delay != sb->max_e2e_delay ||
	    sa->average_latency != sb->average_latency ||
	    sa->network.max_delay != sb->network.max_delay ||
	    sa->network.overloaded_flows != sb->network.overloaded_flows)
		return false;
	for (i = 0; i < a->count; i++) {
		const struct mp_chain_path *p = &a->items[i], *q = &b->items[i];

		if (p->chain_id != q->chain_id || p->latency != q->latency ||
		    p->e2e_delay != q->e2e_delay || p->on_time != q->on_time ||
		    p->reaction_time != q->reaction_time ||
		    p->segment_count != q->segment_count ||
		    p->hop_count != q->hop_count)
			return false;
		for (j = 0; j < p->hop_count; j++)
			if (p->hops[j].type != q->hops[j].type ||
			    p->hops[j].flow.total_delay !=
			    q->hops[j].flow.total_delay ||
			    p->hops[j].flow.jitter != q->hops[j].flow.jitter)
				return false;
	}
	return true;
}

/*
 * The incremental analysis after random moves of fragments between two
 * cores (placement constraints ignored) gives the results of a full
 * analysis.
 */
static void check_incremental(struct mp_design_point *point, uint64_t seed)
{
	struct mp_core_array *cores = &point->cores;
	struct mp_communication_state state;
	struct mp_rng rng;
	int move;

	if (cores->count < 2 ||
	    mp_communication_init(&state, cores, &point->ctx.params.network)) {
		if (cores->count >= 2)
			fail("incremental analysis failed", "init");
		return;
	}
	mp_rng_seed(&rng, seed);
	for (move = 0; move < 12; move++) {
		int from = mp_rng_range(&rng, 0, cores->count - 1);
		int to = mp_rng_range(&rng, 0, cores->count - 1);
		struct mp_core *source = &cores->items[from];
		struct mp_communication_summary summary;
		struct mp_path_array paths;
		struct mp_chain fragment = {0};
		int slot, gcd;

		if (from == to || !source->chains.count)
			continue;
		slot = mp_rng_range(&rng, 0, source->chains.count - 1);
		if (source->chains.items[slot].is_let)
			continue;
		if (mp_chain_copy(&fragment, &source->chains.items[slot]))
			break;
		/* a move the repair could try (the WCETs fit the target) */
		if (mp_core_load_with_chain(&cores->items[to], &fragment, &gcd) >
		    cores->items[to].fill_limit) {
			mp_chain_free(&fragment);
			continue;
		}
		if (mp_core_remove_chain(source, fragment.id,
					 fragment.fragment_index) ||
		    mp_core_add_chain(&cores->items[to], &fragment, gcd) ||
		    mp_core_build_task_list(source, from) ||
		    mp_core_build_task_list(&cores->items[to], to) ||
		    (source->load &&
		     mp_assign_priorities_first_fit(&point->ctx, source)) ||
		    mp_assign_priorities_first_fit(&point->ctx,
						   &cores->items[to])) {
			mp_chain_free(&fragment);
			fail("incremental analysis failed", "move");
			break;
		}
		mp_chain_free(&fragment);
		if (mp_communication_update(&state, cores, from, to) ||
		    mp_communication(cores, &point->ctx.params.network, &paths,
				     &summary)) {
			fail("incremental analysis failed", "update");
			break;
		}
		incremental_moves++;
		if (!same_analysis(&state.paths, &state.summary, &paths,
				   &summary))
			fail("incremental analysis differs from the full one",
			     "seed %llu move %d", (unsigned long long)seed,
			     move);
		mp_path_array_free(&paths);
	}
	mp_communication_state_free(&state);
}

/*
 * Sizing: phi between sigma and the given one; without a target, one run
 * at the given phi; a target reported as met is met.
 */
static void check_sizing(const struct mp_params *params,
			 const struct mp_design_point *point)
{
	const struct mp_sizing *sizing = &point->sizing;

	if (!params->target_scaling) {
		if (sizing->runs != 1 || !sizing->target_met ||
		    sizing->fill_limit != params->fill_limit)
			fail("sizing without a target", "runs %d", sizing->runs);
		return;
	}
	if (sizing->fill_limit > params->fill_limit ||
	    (sizing->fill_limit < params->sigma &&
	     sizing->fill_limit != params->fill_limit) ||
	    point->ctx.params.fill_limit != sizing->fill_limit)
		fail("sized phi out of range", "phi %d sigma %d",
		     sizing->fill_limit, params->sigma);
	if (sizing->target_met &&
	    (!point->feasible || point->stats.scaling_factor * 1000 <
	     params->target_scaling * (1 - 1e-3)))
		fail("sizing target reported met but missed",
		     "scaling %.3f target %d", point->stats.scaling_factor,
		     params->target_scaling);
}

/* check one design point; counts it as checked or unfeasible */
static void check_point(const struct mp_ctx *ctx,
			const struct mp_chain_array *application,
			int *checked, int *unfeasible)
{
	struct mp_design_point point;

	if (mp_run_design_point(ctx, application, NULL, &point)) {
		fail("pipeline returned an error", "sigma %d algorithm %d",
		     ctx->params.sigma, ctx->params.algorithm);
		mp_design_point_free(&point);
		return;
	}
	check_partition(application, &point.fragments, ctx->params.sigma);
	check_sizing(&ctx->params, &point);
	if (!point.feasible) {
		(*unfeasible)++;
		if (getenv("MP_DEBUG"))
			fprintf(stderr, "unfeasible: sigma %d replicas %d zcu memory %d separation %d zcu speed %d fill %d\n",
				ctx->params.sigma, 0, ctx->params.zcu_memory,
				ctx->params.separation, ctx->params.zcu_speed,
				ctx->params.fill_limit);
	} else {
		check_cores(&point.cores, point.fragments.count);
		check_constraints(&ctx->params, &point.cores);
		check_communication(&point, application->count);
		check_incremental(&point, (uint64_t)*checked);
		(*checked)++;
	}
	mp_design_point_free(&point);
}

int main(int argc, char **argv)
{
	static const int sigmas[] = {800000, 400000, 200000, 50000, 20000};
	static const enum mp_algorithm algorithms[] = {MP_BFDU, MP_WFDU,
						       MP_FFDU, MP_CADU};
	int runs = argc > 1 ? atoi(argv[1]) : 20;
	int checked = 0, unfeasible = 0, run, s, a, i;
	struct mp_chain_array application;
	struct mp_ctx ctx;

	for (run = 0; run < runs; run++)
		for (s = 0; s < 5; s++) {
			struct mp_gen_params generator;

			/* a new application for every run and sigma */
			mp_ctx_init(&ctx, 1000 + (uint64_t)run * 5 + s);
			mp_gen_params_default(&generator);
			generator.chain_count = 40 + 20 * s;
			vary_parameters(&ctx.params, &generator, run);
			if (mp_generate_application(&ctx, &generator,
						    &application))
				return 2;
			ctx.params.sigma = sigmas[s];
			for (a = 0; a < 4; a++) {
				ctx.params.algorithm = algorithms[a];
				check_point(&ctx, &application, &checked,
					    &unfeasible);
			}
			mp_chain_array_free(&application);
		}

	/* the detection chain */
	mp_ctx_init(&ctx, 1);
	if (mp_detection_chain_application(&ctx, &application))
		return 2;
	for (a = 0; a < 4; a++) {
		ctx.params.algorithm = algorithms[a];
		check_point(&ctx, &application, &checked, &unfeasible);
	}
	mp_chain_array_free(&application);

	printf("%d mappings checked, %d unfeasible at assignment, %d moves "
	       "analysed incrementally\n", checked, unfeasible,
	       incremental_moves);
	if (!failure_kinds) {
		printf("all invariants hold\n");
		return 0;
	}
	for (i = 0; i < failure_kinds; i++)
		printf("FAIL x%-5d %s\n           e.g. %s\n", failures[i].count,
		       failures[i].what, failures[i].example);
	return 1;
}
