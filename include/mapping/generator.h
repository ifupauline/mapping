/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MAPPING_GENERATOR_H
#define MAPPING_GENERATOR_H

#include "mapping/context.h"
#include "mapping/model.h"

/*
 * Applications: the automotive generator and the detection chain.
 *
 * A generated task-chain follows the rules of the WATERS 2015 benchmark
 * for cause-effect chains (Kramer, Ziegenbein, Hamann): 1, 2 or 3
 * activation patterns (distinct periods, 70/20/10 %), 2 to 5 tasks per
 * pattern (30/40/20/10 %), periods with the shares of the benchmark. The
 * utilization of a chain is drawn log-uniformly (most chains are light, a
 * few heavy chains model perception or planning) and split over its tasks
 * with UUniFast. Chains interact with the physical world through the zones:
 * a chain can be local to one zone, or start with a sensor task and end
 * with an actuator task bound to a zone controller.
 */

#define MP_GEN_MAX_TASKS	15	/* 3 patterns x 5 tasks */
#define MP_GEN_FRAME_BYTES	1500	/* largest one-frame message */

/* parameters of the generator (defaults: mp_gen_params_default) */
struct mp_gen_params {
	int chain_count;		/* task-chains of the application (300) */
	int local_percent;		/* chains whose tasks all run in one
					 * zone (25) */
	int sensor_percent;		/* other chains: first task bound to a
					 * zone, its sensor (60) */
	int actuator_percent;		/* last task bound to a zone, its
					 * actuator (50) */
	int io_percent;			/* share of the chain utilization of
					 * each sensor or actuator task (5) */
	int min_chain_utilization;	/* ppm of the reference core (1000) */
	int max_chain_utilization;	/* ppm (80000), log-uniform between */
	int heavy_percent;		/* heavy chains, perception and
					 * planning (2) */
	int max_heavy_utilization;	/* ppm, uniform from max_chain (800000) */
	int max_task_utilization;	/* largest task, ppm (300000): it still
					 * fits a ZCU core at half speed */
	int small_message_percent;	/* messages of 4 to 64 bytes (70) */
	int medium_message_percent;	/* 65 to 1500 bytes, one frame (25) */
	int large_message_bytes;	/* the rest: 1501 bytes up to this,
					 * sensor data (16000) */
	int deadline_percent;		/* end-to-end deadline, % of the sum of
					 * the periods of its activation
					 * patterns; 0 = none (300) */
	int min_task_memory;		/* memory of a task, KB: log-uniform */
	int max_task_memory;		/* between these (4 and 256) */
	int colocate_percent;		/* two consecutive free tasks of a
					 * pattern must share a core (0) */
	int zone_only_percent;		/* chains (not local) whose free tasks
					 * may only run on the zone
					 * controllers (0) */
	int replica_percent;		/* chains that are replicas of others,
					 * at most 50 (0) */
	int type_variation;		/* WCET of a task on each core type:
					 * up to this % above or below its
					 * speed ratio (0) */
	int lockstep_percent;		/* chains (safety functions) that may
					 * only run on lock-step cores (0) */
};

/* the default parameters, documented in struct mp_gen_params */
void mp_gen_params_default(struct mp_gen_params *params);

/*
 * mp_draw_waters_period() - a period with the shares of the WATERS 2015
 * benchmark (angle-synchronous runnables left out), microseconds
 */
int mp_draw_waters_period(struct mp_rng *rng);

/*
 * mp_uunifast() - split @total over @count values with UUniFast (Bini and
 * Buttazzo, 2005): uniform over the simplex of sums equal to @total
 */
void mp_uunifast(struct mp_rng *rng, double total, int count, double *shares);

/*
 * mp_generate_application() - synthetic application; chains sorted by
 * decreasing utilization and numbered in that order. Sets
 * ctx->params.chain_count. Return: 0, -EINVAL (inconsistent parameters)
 * or -ENOMEM.
 */
int mp_generate_application(struct mp_ctx *ctx,
			    const struct mp_gen_params *params,
			    struct mp_chain_array *application);

/*
 * mp_detection_chain_application() - the detection chain: a small
 * illustrative ADAS chain of 9 tasks pinned to the central computer (not
 * taken from a benchmark).
 */
int mp_detection_chain_application(struct mp_ctx *ctx,
			      struct mp_chain_array *application);

#endif /* MAPPING_GENERATOR_H */
