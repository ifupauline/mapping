/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MAPPING_LIFECYCLE_H
#define MAPPING_LIFECYCLE_H

#include "mapping/generator.h"
#include "mapping/pipeline.h"

/*
 * Life of a vehicle platform: the hardware is sized for the software at
 * the start of production (SOP), then software updates add task-chains
 * to that hardware.
 *
 * SOP: the methodology maps the initial application at the filling limit
 * of the SOP (a lower limit buys cores, hence room for later functions).
 * The platform is then fixed: the cores the mapping uses, nothing more.
 *
 * Update u: new task-chains, generated like the initial ones, are
 * partitioned and assigned to the cores of the platform (no core is
 * added, the fragments already there stay where they are), up to the
 * filling limit of the updates; the priorities are assigned again on
 * every core. The update is absorbed when every new fragment found a core
 * and every core is schedulable; the end-to-end timing of every
 * task-chain is analysed again. The first update that is not absorbed
 * ends the life of the platform.
 */

#define MP_LIFECYCLE_MAX_UPDATES	10

struct mp_lifecycle_params {
	int updates;		/* software updates, 0 = no lifecycle */
	int chains_per_update;	/* task-chains added by each update (15) */
	int update_fill_limit;	/* filling limit of the cores during the
				 * updates, ppm (800000) */
};

/* the platform after the SOP and after each absorbed update */
struct mp_lifecycle_result {
	int absorbed;		/* updates absorbed before the first that is
				 * not (or all of them) */
	int platform_cores;	/* cores of the platform */
	/* index 0: SOP, then after update u (absorbed ones only) */
	int chains[MP_LIFECYCLE_MAX_UPDATES + 1];
	float scaling[MP_LIFECYCLE_MAX_UPDATES + 1];	/* critical WCET
							 * scaling factor */
	float load[MP_LIFECYCLE_MAX_UPDATES + 1];	/* application load,
							 * cores */
	float on_time[MP_LIFECYCLE_MAX_UPDATES + 1];	/* % task-chains */
	int misses[MP_LIFECYCLE_MAX_UPDATES + 1];	/* deadline misses */
};

void mp_lifecycle_params_default(struct mp_lifecycle_params *params);

/*
 * mp_run_lifecycle() - the SOP and the updates of one platform
 * @base: parameters of the SOP mapping (its fill_limit is the one of the
 *	  SOP)
 * @application: the initial task-chains
 * @generator: how to generate the task-chains of the updates
 * @seed: seed of the updates (update u is generated from a seed derived
 *	  from @seed and u)
 * @sop: set to the design point of the SOP (free it)
 *
 * Return: 0 or a negative errno. An SOP mapping that is not feasible
 * absorbs no update.
 */
int mp_run_lifecycle(const struct mp_ctx *base,
		     const struct mp_chain_array *application,
		     const struct mp_gen_params *generator,
		     const struct mp_lifecycle_params *params, uint64_t seed,
		     struct mp_design_point *sop,
		     struct mp_lifecycle_result *result);

#endif /* MAPPING_LIFECYCLE_H */
