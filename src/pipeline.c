// SPDX-License-Identifier: Apache-2.0
/* One design point through the whole methodology (see pipeline.h). */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include "mapping/assign.h"
#include "mapping/partition.h"
#include "mapping/pipeline.h"
#include "mapping/placement.h"
#include "mapping/repair.h"
#include "mapping/sched.h"

double mp_thread_cpu_seconds(void)
{
	struct timespec now;

	if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &now))
		return 0;
	return (double)now.tv_sec + (double)now.tv_nsec * 1e-9;
}

const char *mp_algorithm_name(enum mp_algorithm algorithm)
{
	switch (algorithm) {
	case MP_BFDU:
		return "BFDU";
	case MP_WFDU:
		return "WFDU";
	case MP_CADU:
		return "CADU";
	default:
		return "FFDU";
	}
}

int mp_parse_algorithm(const char *name, enum mp_algorithm *algorithm)
{
	static const enum mp_algorithm all[] = {MP_BFDU, MP_WFDU, MP_FFDU,
						MP_CADU};
	int i, k;

	for (i = 0; i < 4; i++) {
		const char *known = mp_algorithm_name(all[i]);

		for (k = 0; known[k] && name[k]; k++)
			if ((name[k] | 0x20) != (known[k] | 0x20))
				break;
		if (!known[k] && !name[k]) {
			*algorithm = all[i];
			return 0;
		}
	}
	return -EINVAL;
}

static void notify(const struct mp_observer *observer, const char *stage,
		   const struct mp_design_point *point)
{
	if (observer && observer->phase_done)
		observer->phase_done(observer->data, stage, point);
}

/* the time since *@start, which is moved to now */
static double lap(double *start)
{
	double now = mp_thread_cpu_seconds(), elapsed = now - *start;

	*start = now;
	return elapsed;
}

/* every WCET of @fragments increased by @margin percent (rounded up) */
static void apply_wcet_margin(struct mp_chain_array *fragments, int margin)
{
	int i, j;

	if (!margin)
		return;
	for (i = 0; i < fragments->count; i++) {
		struct mp_chain *chain = &fragments->items[i];

		chain->utilization = 0;
		for (j = 0; j < chain->tasks.count; j++) {
			struct mp_task *task = &chain->tasks.items[j];
			int64_t wcet = ((int64_t)task->reference_wcet *
					(100 + margin) + 99) / 100;

			if (wcet >= task->period)
				wcet = task->period - 1;
			task->reference_wcet = (int)wcet;
			task->wcet = (int)wcet;
			task->utilization = mp_utilization(task->wcet,
							   task->period);
			chain->utilization += task->utilization;
		}
	}
}

/*
 * contention: the tasks that give no count of memory requests get one
 * from their WCET (before any margin, which is execution time)
 */
static void set_accesses(const struct mp_params *params,
			 struct mp_chain_array *chains)
{
	int i, j;

	if (!params->memory_latency)
		return;
	for (i = 0; i < chains->count; i++)
		for (j = 0; j < chains->items[i].tasks.count; j++) {
			struct mp_task *task = &chains->items[i].tasks.items[j];

			if (!task->accesses)
				task->accesses = mp_default_accesses(params,
					task->reference_wcet ?
					task->reference_wcet : task->wcet);
		}
}

void mp_prepare_chains(const struct mp_params *params,
		       struct mp_chain_array *chains)
{
	set_accesses(params, chains);
	apply_wcet_margin(chains, params->wcet_margin);
}

/*
 * one run of the methodology at the parameters of @base; with contention,
 * stops after the assignment if it took more cores on a controller than
 * could compete for its memory: @grown is set, @competing has the counts
 */
static int run_mapping(const struct mp_ctx *base,
		       const struct mp_chain_array *application,
		       const struct mp_observer *observer,
		       struct mp_design_point *point, int *competing,
		       bool *grown)
{
	struct mp_ctx *ctx = &point->ctx;
	struct mp_phase_times *times = &point->times;
	double clock, begin;
	int err;

	memset(point, 0, sizeof(*point));
	*ctx = *base;
	point->sigma = ctx->params.sigma;
	point->algorithm = ctx->params.algorithm;
	begin = clock = mp_thread_cpu_seconds();

	/* partitioning, after the WCET margin of the design */
	err = mp_chain_array_copy(&point->fragments, application);
	if (!err) {
		mp_prepare_chains(&ctx->params, &point->fragments);
		err = mp_partition(ctx, &point->fragments);
	}
	if (err)
		return err;
	times->partitioning = lap(&clock);
	notify(observer, "partitioned", point);

	/* assignment on the zonal architecture */
	err = mp_architecture_init(ctx, &point->cores);
	if (!err)
		err = mp_assign(ctx, &point->fragments, &point->cores);
	if (err)
		return err;
	/*
	 * contention: the assignment took more cores on a controller than
	 * it assumed could compete for its memory; the caller runs it again
	 * with that many
	 */
	if (ctx->feasible && ctx->params.memory_latency) {
		int count[MP_MAX_ECUS] = {0}, i;

		for (i = 0; i < point->cores.count; i++)
			count[point->cores.items[i].controller]++;
		for (i = 0; i < mp_ecu_count(&ctx->params); i++)
			if (count[i] > mp_competing_cores(&ctx->params, i)) {
				competing[i] = count[i];
				*grown = true;
			}
		if (*grown)
			return 0;
	}
	times->assignment = lap(&clock);
	point->feasible = ctx->feasible;
	if (!point->feasible) {
		times->total = clock - begin;
		return 0;
	}
	/* consolidation: cores whose fragments fit elsewhere are emptied */
	err = mp_consolidation(ctx, &point->cores, &ctx->emptied_cores);
	if (err)
		return err;
	notify(observer, "assigned", point);

	/* scheduling: priority assignment */
	err = mp_schedule(ctx, &point->cores);
	if (err)
		return err;
	times->scheduling = lap(&clock);
	notify(observer, "scheduled", point);

	/* placement: swapping, then displacement */
	err = ctx->params.swap_moves ? mp_swapping(ctx, &point->cores) : 0;
	if (err)
		return err;
	mp_count_schedulable_cores(ctx, &point->cores);
	ctx->rate_after_swapping = ctx->schedulable_rate;
	times->swapping = lap(&clock);

	err = ctx->params.relocation_moves ?
	      mp_displacement(ctx, &point->cores) : 0;
	if (err)
		return err;
	mp_count_schedulable_cores(ctx, &point->cores);
	ctx->rate_after_displacement = ctx->schedulable_rate;
	times->displacement = lap(&clock);
	/* the cores left without a task (also when the moves are off) */
	{
		int i;

		for (i = 0; i < point->cores.count; i++)
			point->cores.items[i].is_empty =
				point->cores.items[i].load == 0;
	}

	/* timing constraints: moves for the chains past their deadline */
	if (ctx->params.repair_deadlines) {
		err = mp_repair_deadlines(ctx, &point->cores);
		if (err)
			return err;
	}
	/* contention from the cores that run tasks only */
	err = mp_contention_update(ctx, &point->cores);
	if (err)
		return err;
	mp_count_schedulable_cores(ctx, &point->cores);
	ctx->rate_final = ctx->schedulable_rate;
	times->repair = lap(&clock);
	notify(observer, "placed", point);

	/* communication and metrics */
	err = mp_communication(&point->cores, &ctx->params.network,
			       &point->paths,
			       &point->communication);
	if (err)
		return err;
	times->communication = lap(&clock);

	err = mp_verify_mapping(&point->cores);
	if (err)
		return err;
	mp_compute_stats(ctx, &point->cores, &point->stats);
	point->stats.lower_bound_cores =
		mp_cores_lower_bound(&ctx->params, &point->fragments);
	times->total = clock - begin;
	return 0;
}

/*
 * run_mapping() until the cores that compete for the memory of each
 * controller are as many as the mapping needs (a fixed point: more cores,
 * more contention, longer WCETs, possibly more cores); not found within
 * 8 runs: infeasible
 */
static int run_once(const struct mp_ctx *base,
		    const struct mp_chain_array *application,
		    const struct mp_observer *observer,
		    struct mp_design_point *point)
{
	struct mp_ctx ctx = *base;
	int round, err;

	for (round = 0;; round++) {
		bool grown = false;

		err = run_mapping(&ctx, application, observer, point,
				  ctx.params.competing_cores, &grown);
		if (err || !grown)
			return err;
		if (round == 7) {
			point->feasible = false;
			point->ctx.feasible = false;
			return 0;
		}
		mp_design_point_free(point);
	}
}

/*
 * whether @point reaches the scaling factor @target (permille): every
 * core schedulable (the factor is computed over the schedulable cores)
 * and the factor at least the target (found by bisection to 0.1 %)
 */
static bool reaches(const struct mp_design_point *point, int target)
{
	return point->feasible && !point->stats.unschedulable_cores &&
	       point->stats.scaling_factor * 1000 >= target - 1e-3 * target;
}

int mp_run_design_point(const struct mp_ctx *base,
			const struct mp_chain_array *application,
			const struct mp_observer *observer,
			struct mp_design_point *point)
{
	int target = base->params.target_scaling, low, high, step, err;
	struct mp_design_point best, trial;
	struct mp_ctx ctx = *base;
	double begin = mp_thread_cpu_seconds();

	err = run_once(&ctx, application, observer, point);
	point->sizing.fill_limit = ctx.params.fill_limit;
	point->sizing.target_met = !target || reaches(point, target);
	point->sizing.runs = 1;
	if (err || point->sizing.target_met ||
	    ctx.params.fill_limit <= ctx.params.sigma)
		goto out;

	/* phi = sigma: if even that misses the target, it is the answer */
	low = ctx.params.sigma;
	high = ctx.params.fill_limit;
	ctx.params.fill_limit = low;
	err = run_once(&ctx, application, observer, &best);
	if (err) {
		mp_design_point_free(&best);
		goto out;
	}
	mp_design_point_free(point);
	*point = best;
	point->sizing.runs = 2;
	point->sizing.fill_limit = low;
	point->sizing.target_met = reaches(point, target);
	if (!point->sizing.target_met)
		goto out;

	/* the largest phi in [low, high) that reaches the target */
	for (step = 0; step < 7; step++) {
		int middle = low + (high - low) / 2, runs = point->sizing.runs;

		ctx.params.fill_limit = middle;
		err = run_once(&ctx, application, observer, &trial);
		if (err) {
			mp_design_point_free(&trial);
			goto out;
		}
		if (reaches(&trial, target)) {
			low = middle;
			mp_design_point_free(point);
			*point = trial;
			point->sizing.fill_limit = middle;
			point->sizing.target_met = true;
		} else {
			high = middle;
			mp_design_point_free(&trial);
		}
		point->sizing.runs = runs + 1;
	}
out:
	point->sizing.cpu_time = mp_thread_cpu_seconds() - begin;
	return err;
}

void mp_design_point_free(struct mp_design_point *point)
{
	mp_chain_array_free(&point->fragments);
	mp_core_array_free(&point->cores);
	mp_path_array_free(&point->paths);
}
