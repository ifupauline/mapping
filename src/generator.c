// SPDX-License-Identifier: Apache-2.0
/* Applications: automotive generator and the detection chain (see generator.h). */
#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#include "mapping/generator.h"

void mp_gen_params_default(struct mp_gen_params *params)
{
	params->chain_count = 300;
	params->local_percent = 25;
	params->sensor_percent = 60;
	params->actuator_percent = 50;
	params->io_percent = 5;
	params->min_chain_utilization = 1000;
	params->max_chain_utilization = 80000;
	params->heavy_percent = 2;
	params->max_heavy_utilization = 800000;
	params->max_task_utilization = 300000;
	params->small_message_percent = 70;
	params->medium_message_percent = 25;
	params->large_message_bytes = 16000;
	params->deadline_percent = 300;
	params->min_task_memory = 4;
	params->max_task_memory = 256;
	params->colocate_percent = 0;
	params->zone_only_percent = 0;
	params->replica_percent = 0;
	params->type_variation = 0;
	params->lockstep_percent = 0;
}

/*
 * WATERS 2015 periods and their share of the runnables, percent (Kramer,
 * Ziegenbein, Hamann, "Real world automotive benchmarks for free"). The
 * 15 % of angle-synchronous runnables have no fixed period and are left
 * out: the shares below sum to 85.
 */
static const struct {
	int period;		/* microseconds */
	int share;		/* percent of the runnables */
} waters_periods[] = {
	{    1000,  3 }, {    2000,  2 }, {    5000,  2 }, {   10000, 25 },
	{   20000, 25 }, {   50000,  3 }, {  100000, 20 }, {  200000,  1 },
	{ 1000000,  4 },
};

#define WATERS_PERIOD_COUNT \
	((int)(sizeof(waters_periods) / sizeof(waters_periods[0])))

/* a value of @weights (shares in percent, summing to 100), as an index */
static int draw_weighted(struct mp_rng *rng, const int *weights, int count)
{
	int draw = mp_rng_range(rng, 0, 99), i;

	for (i = 0; i < count - 1; i++) {
		if (draw < weights[i])
			return i;
		draw -= weights[i];
	}
	return count - 1;
}

int mp_draw_waters_period(struct mp_rng *rng)
{
	int total = 0, draw, i;

	for (i = 0; i < WATERS_PERIOD_COUNT; i++)
		total += waters_periods[i].share;
	draw = mp_rng_range(rng, 0, total - 1);
	for (i = 0; i < WATERS_PERIOD_COUNT; i++) {
		if (draw < waters_periods[i].share)
			return waters_periods[i].period;
		draw -= waters_periods[i].share;
	}
	return waters_periods[WATERS_PERIOD_COUNT - 1].period;
}

void mp_uunifast(struct mp_rng *rng, double total, int count, double *shares)
{
	double remaining = total;
	int i;

	for (i = 0; i < count - 1; i++) {
		/* the sum of the count - 1 - i values left after this one */
		double next = remaining * pow(mp_rng_uniform(rng),
					      1.0 / (count - 1 - i));

		shares[i] = remaining - next;
		remaining = next;
	}
	shares[count - 1] = remaining;
}

/* message of a task: mostly small signals, some frames, a few large */
static int draw_message_bytes(struct mp_rng *rng,
			      const struct mp_gen_params *params)
{
	int class = mp_rng_range(rng, 0, 99);

	if (class < params->small_message_percent)
		return mp_rng_range(rng, 4, 64);
	if (class < params->small_message_percent +
		    params->medium_message_percent)
		return mp_rng_range(rng, 65, MP_GEN_FRAME_BYTES);
	return mp_rng_range(rng, MP_GEN_FRAME_BYTES + 1,
			    params->large_message_bytes);
}

/* an ECU that sensors and actuators are wired to (a zone controller of
 * the zonal platform), uniformly among those of @io */
static int draw_io_ecu(struct mp_rng *rng, unsigned io)
{
	int count = 0, pick, c;

	for (c = 0; c < MP_MAX_ECUS; c++)
		count += (io >> c) & 1;
	pick = mp_rng_range(rng, 0, count - 1);
	for (c = 0; c < MP_MAX_ECUS; c++)
		if (((io >> c) & 1) && !pick--)
			return c;
	return 0;
}

/*
 * Structure of a chain (WATERS 2015 rules for cause-effect chains): the
 * period of each task, in chain order. Returns the number of tasks and
 * sets @pattern_period_sum to the sum of the periods of its patterns.
 */
static int draw_chain_structure(struct mp_rng *rng, int *periods,
				int64_t *pattern_period_sum)
{
	static const int pattern_weights[] = {70, 20, 10};	/* 1, 2, 3 */
	static const int task_weights[] = {30, 40, 20, 10};	/* 2 to 5 */
	int patterns = draw_weighted(rng, pattern_weights, 3) + 1;
	int chosen[3], count = 0, p, k;

	*pattern_period_sum = 0;
	for (p = 0; p < patterns; p++) {
		int tasks = draw_weighted(rng, task_weights, 4) + 2;
		bool distinct;

		/* activation patterns have distinct periods */
		do {
			chosen[p] = mp_draw_waters_period(rng);
			distinct = true;
			for (k = 0; k < p; k++)
				distinct &= chosen[k] != chosen[p];
		} while (!distinct);
		*pattern_period_sum += chosen[p];
		for (k = 0; k < tasks; k++)
			periods[count++] = chosen[p];
	}
	return count;
}

/* utilization of a chain, ppm: log-uniform, or a heavy chain */
static double draw_chain_utilization(struct mp_rng *rng,
				     const struct mp_gen_params *params)
{
	double low = params->min_chain_utilization;
	double high = params->max_chain_utilization;
	double utilization;

	if (mp_rng_range(rng, 0, 99) < params->heavy_percent)
		utilization = high + mp_rng_uniform(rng) *
			      (params->max_heavy_utilization - high);
	else
		utilization = low * pow(high / low, mp_rng_uniform(rng));
	return utilization;
}

/*
 * Memory of every task (log-uniform), co-location of consecutive free
 * tasks of one pattern, and chains whose free tasks may only run on the
 * ECUs of @io, the zone controllers (safety functions on their lock-step
 * cores).
 */
static void add_constraints(struct mp_rng *rng,
			    const struct mp_gen_params *params,
			    struct mp_chain *chain, bool local, unsigned io)
{
	bool zone_only = !local &&
			 mp_rng_range(rng, 0, 99) < params->zone_only_percent;
	bool lockstep = params->lockstep_percent &&
			mp_rng_range(rng, 0, 99) < params->lockstep_percent;
	double spread = log(1 + params->type_variation / 100.0);
	int k, t;

	for (k = 0; k < chain->tasks.count; k++) {
		struct mp_task *task = &chain->tasks.items[k];

		task->memory = (int)lround(params->min_task_memory *
			pow((double)params->max_task_memory /
			    params->min_task_memory, mp_rng_uniform(rng)));
		chain->memory += task->memory;
		if (zone_only && task->controller == MP_CONTROLLER_ANY)
			task->allowed = io;
		/*
		 * a task runs faster or slower than its speed ratio on each
		 * type of core: factors log-uniform within (1 + v)^-1 and
		 * 1 + v; lock-step cores share the microcontroller design
		 */
		if (params->type_variation) {
			for (t = 0; t < MP_CORE_LOCKSTEP; t++)
				task->type_factor[t] = (short)lround(100 *
					exp(spread * (2 * mp_rng_uniform(rng) - 1)));
			task->type_factor[MP_CORE_LOCKSTEP] =
				task->type_factor[MP_CORE_MCU];
		}
		/* safety functions (ASIL D) only on lock-step cores */
		if (lockstep)
			task->types = 1u << MP_CORE_LOCKSTEP;
	}
	for (k = 0; k + 1 < chain->tasks.count; k++) {
		const struct mp_task *next = &chain->tasks.items[k + 1];
		struct mp_task *task = &chain->tasks.items[k];

		if (task->controller == MP_CONTROLLER_ANY &&
		    next->controller == MP_CONTROLLER_ANY &&
		    task->period == next->period &&
		    mp_rng_range(rng, 0, 99) < params->colocate_percent)
			task->colocate_next = true;
	}
}

/* the free tasks of @chain may not run where @other has bound tasks */
static void exclude_zones(struct mp_chain *chain, const struct mp_chain *other)
{
	unsigned zones = 0;
	int k;

	for (k = 0; k < other->tasks.count; k++)
		if (other->tasks.items[k].controller != MP_CONTROLLER_ANY)
			zones |= MP_CONTROLLER_BIT(other->tasks.items[k].controller);
	for (k = 0; k < chain->tasks.count; k++) {
		struct mp_task *task = &chain->tasks.items[k];

		if (task->controller == MP_CONTROLLER_ANY)
			task->allowed = mp_task_allowed(task) & ~zones;
	}
}

/*
 * A replica of @original in @replica (a deep copy): same tasks, its sensor
 * and actuator in zones the original does not use (redundant sensors and
 * actuators), the same replica group.
 */
static int make_replica(struct mp_rng *rng, unsigned io,
			struct mp_chain *original, struct mp_chain *replica,
			int group)
{
	int map[MP_MAX_ECUS], k, err;
	unsigned used = 0;

	err = mp_chain_copy(replica, original);
	if (err)
		return err;
	original->replica_group = group;
	replica->replica_group = group;
	for (k = 0; k < MP_MAX_ECUS; k++)
		map[k] = -1;
	for (k = 0; k < original->tasks.count; k++)
		if (original->tasks.items[k].controller != MP_CONTROLLER_ANY)
			used |= 1u << original->tasks.items[k].controller;
	for (k = 0; k < replica->tasks.count; k++) {
		struct mp_task *task = &replica->tasks.items[k];
		int zone;

		if (task->controller == MP_CONTROLLER_ANY)
			continue;
		if (map[task->controller] < 0) {
			/* an ECU used by neither chain yet, if any is left */
			if (io & ~used) {
				do
					zone = draw_io_ecu(rng, io);
				while (used & (1u << zone));
			} else {
				zone = task->controller;
			}
			used |= 1u << zone;
			map[task->controller] = zone;
		}
		task->controller = map[task->controller];
	}
	if (replica->controller != MP_CONTROLLER_ANY)
		replica->controller = map[replica->controller];
	/* the free tasks of each stay out of the zones the other is bound to */
	exclude_zones(original, replica);
	exclude_zones(replica, original);
	return 0;
}

/* one chain: structure, utilizations, zones, messages, deadline */
static int generate_chain(struct mp_ctx *ctx,
			  const struct mp_gen_params *params,
			  struct mp_chain *chain)
{
	double shares[MP_GEN_MAX_TASKS], utilization;
	int periods[MP_GEN_MAX_TASKS], zones[MP_GEN_MAX_TASKS];
	struct mp_rng *rng = &ctx->rng;
	unsigned io = mp_io_ecus(&ctx->params);
	double cap;
	int task_count, attempt, k, zone, io_count, err;
	int64_t pattern_period_sum;
	bool local;

	memset(chain, 0, sizeof(*chain));
	task_count = draw_chain_structure(rng, periods, &pattern_period_sum);
	utilization = draw_chain_utilization(rng, params);

	/* zones: a local chain, or a sensor and an actuator task; heavy
	 * chains (perception, planning) are never local */
	local = mp_rng_range(rng, 0, 99) < params->local_percent &&
		utilization <= params->max_chain_utilization;
	zone = draw_io_ecu(rng, io);
	chain->controller = local ? zone : MP_CONTROLLER_ANY;
	for (k = 0; k < task_count; k++)
		zones[k] = local ? zone : MP_CONTROLLER_ANY;
	if (!local) {
		if (mp_rng_range(rng, 0, 99) < params->sensor_percent)
			zones[0] = draw_io_ecu(rng, io);
		if (mp_rng_range(rng, 0, 99) < params->actuator_percent)
			zones[task_count - 1] = draw_io_ecu(rng, io);
	}

	/*
	 * Sensor and actuator tasks are input/output drivers: each takes
	 * io_percent of the chain utilization, the computation tasks share the
	 * rest with UUniFast-discard (a split with a task above the cap is
	 * drawn again).
	 */
	io_count = 0;
	if (!local)
		for (k = 0; k < task_count; k++)
			io_count += zones[k] != MP_CONTROLLER_ANY;
	if (io_count == task_count) {
		/* a sensor and an actuator only: the actuator side computes */
		zones[task_count - 1] = MP_CONTROLLER_ANY;
		io_count--;
	}
	/* the computation tasks must have room below their cap */
	cap = 0.8 * (task_count - io_count) * params->max_task_utilization /
	      (1 - io_count * params->io_percent / 100.0);
	if (utilization > cap)
		utilization = cap;
	for (attempt = 0; attempt < 1000; attempt++) {
		double compute = utilization * (1 - io_count *
						params->io_percent / 100.0);
		double free_shares[MP_GEN_MAX_TASKS];
		bool too_heavy = false;
		int f = 0;

		mp_uunifast(rng, compute, task_count - io_count, free_shares);
		for (k = 0; k < task_count; k++) {
			bool driver = io_count && zones[k] != MP_CONTROLLER_ANY;

			shares[k] = driver ? utilization * params->io_percent /
				    100.0 : free_shares[f++];
			too_heavy |= shares[k] > params->max_task_utilization;
		}
		if (!too_heavy)
			break;
	}

	for (k = 0; k < task_count; k++) {
		struct mp_task task;

		memset(&task, 0, sizeof(task));
		task.period = periods[k];
		/* C = U x T, at least 1 us and below the period */
		task.wcet = (int)lround(shares[k] < 0 ? 0 :
					shares[k] * task.period /
					MP_UTILIZATION_UNIT);
		if (task.wcet < 1)
			task.wcet = 1;
		if (task.wcet >= task.period)
			task.wcet = task.period - 1;
		task.reference_wcet = task.wcet;
		task.utilization = mp_utilization(task.wcet, task.period);
		task.id_in_chain = k;
		task.priority = k + 1;
		task.message_bytes = draw_message_bytes(rng, params);
		task.controller = zones[k];
		err = mp_task_array_push(&chain->tasks, &task);
		if (err) {
			mp_chain_free(chain);
			return err;
		}
		chain->utilization += task.utilization;
	}
	chain->deadline = (int)(pattern_period_sum * params->deadline_percent /
				100);
	add_constraints(rng, params, chain, local, io);
	return 0;
}

/* ids in array order, unique task ids, chain count */
static void number_chains(struct mp_ctx *ctx, struct mp_chain_array *chains)
{
	int unique_id = 0;
	int i, j;

	for (i = 0; i < chains->count; i++) {
		struct mp_chain *chain = &chains->items[i];

		chain->id = i;
		for (j = 0; j < chain->tasks.count; j++) {
			chain->tasks.items[j].chain_id = i;
			chain->tasks.items[j].unique_id = unique_id++;
		}
		chain->period_gcd = mp_tasks_period_gcd(&chain->tasks);
	}
	ctx->params.chain_count = chains->count;
}

int mp_generate_application(struct mp_ctx *ctx,
			    const struct mp_gen_params *params,
			    struct mp_chain_array *application)
{
	struct mp_chain chain;
	int replicas, originals, i, err;

	memset(application, 0, sizeof(*application));
	replicas = params->chain_count * params->replica_percent / 100;
	originals = params->chain_count - replicas;
	if (params->chain_count < 1 || params->min_chain_utilization < 1 ||
	    params->replica_percent < 0 || params->replica_percent > 50 ||
	    params->min_task_memory < 1 ||
	    params->max_task_memory < params->min_task_memory ||
	    params->max_chain_utilization < params->min_chain_utilization ||
	    params->max_heavy_utilization < params->max_chain_utilization ||
	    params->max_task_utilization < 1)
		return -EINVAL;

	for (i = 0; i < originals; i++) {
		err = generate_chain(ctx, params, &chain);
		if (!err)
			err = mp_chain_array_push(application, &chain);
		mp_chain_free(&chain);
		if (err)
			goto err_free_application;
	}
	/*
	 * replicas of distinct originals drawn at random among the control
	 * chains (heavy perception chains are not replicated: a replica
	 * runs on other controllers, possibly the slower zone ones)
	 */
	for (i = 0; i < replicas; i++) {
		struct mp_chain *original;
		int pick, tries = 0;

		do
			pick = mp_rng_range(&ctx->rng, 0, originals - 1);
		while ((application->items[pick].replica_group ||
			application->items[pick].utilization >
			params->max_chain_utilization) && ++tries < 100000);
		original = &application->items[pick];
		err = make_replica(&ctx->rng, mp_io_ecus(&ctx->params), original,
				   &chain, i + 1);
		if (!err)
			err = mp_chain_array_push(application, &chain);
		mp_chain_free(&chain);
		if (err)
			goto err_free_application;
	}
	err = mp_sort_chains_by_decreasing_utilization(application);
	if (err)
		goto err_free_application;
	number_chains(ctx, application);
	return 0;

err_free_application:
	mp_chain_array_free(application);
	return err;
}

/*
 * the detection chain: a small illustrative ADAS chain of nine tasks (it
 * is not taken from a benchmark): WCET and period in microseconds,
 * message sizes in bytes
 */
static const struct {
	const char *name;
	int wcet;
	int period;
	int message_bytes;
} detection_chain_tasks[] = {
	{ "gps_grabber",    7000, 50000,     64 },
	{ "lidar_grabber", 12000, 50000, 16000 },
	{ "cam_grabber",    2000, 25000, 16000 },
	{ "localization",  28000, 50000,   256 },
	{ "detection",     28000, 50000,  1024 },
	{ "ekf",            6500, 10000,   256 },
	{ "fusion",        25000, 50000,  1024 },
	{ "planner",        5000, 10000,   256 },
	{ "control",        4500, 10000,     64 },
};

int mp_detection_chain_application(struct mp_ctx *ctx,
			      struct mp_chain_array *application)
{
	struct mp_chain chain;
	int order[MP_MAX_ECUS], central;
	size_t i;
	int err;

	/* on the first central computer (the PCU) */
	mp_ecu_preference(&ctx->params, order);
	central = order[0];
	memset(application, 0, sizeof(*application));
	memset(&chain, 0, sizeof(chain));
	chain.controller = central;

	for (i = 0; i < sizeof(detection_chain_tasks) / sizeof(detection_chain_tasks[0]);
	     i++) {
		struct mp_task task;

		memset(&task, 0, sizeof(task));
		task.wcet = detection_chain_tasks[i].wcet;
		task.reference_wcet = task.wcet;
		task.period = detection_chain_tasks[i].period;
		task.utilization = mp_utilization(task.wcet, task.period);
		task.message_bytes = detection_chain_tasks[i].message_bytes;
		task.controller = central;
		task.id_in_chain = (int)i;
		task.priority = (int)i + 1;
		err = mp_task_array_push(&chain.tasks, &task);
		if (err)
			goto err_free_chain;
		chain.utilization += task.utilization;
	}
	/* no deadline */
	err = mp_chain_array_push(application, &chain);
	if (err)
		goto err_free_chain;
	mp_chain_free(&chain);
	number_chains(ctx, application);
	return 0;

err_free_chain:
	mp_chain_free(&chain);
	return err;
}
