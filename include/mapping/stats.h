/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MAPPING_STATS_H
#define MAPPING_STATS_H

#include "mapping/context.h"
#include "mapping/model.h"

/*
 * Performance metrics of a mapping.
 *
 * Utilizations are in cores: 1.0 is one core fully busy. Rates are in
 * percent.
 */
/* sizing of one controller */
struct mp_controller_stats {
	int used_cores;
	float application_utilization;	/* cores */
	float let_utilization;		/* cores */
	int memory;			/* used by its tasks, KB */
	float scaling_factor;		/* smallest critical WCET scaling
					 * factor of its schedulable cores
					 * (-1: none) */
};

struct mp_stats {
	int used_cores;		/* M: cores running application tasks */
	int lower_bound_cores;	/* a lower bound of M for these fragments
				 * (mp_cores_lower_bound) */
	int central_cores;	/* of which on the central computers */
	int other_cores;	/* of which on the other ECUs */
	int type_cores[MP_CORE_TYPES];	/* of each type of core */
	int task_count;		/* application tasks (LET tasks excluded) */
	int schedulable_cores;	/* after placement (all cores, empty ones
				 * included) */
	int unschedulable_cores;

	float rate_scheduling;	/* SR after priority assignment, % */
	float rate_swapping;	/* SR after swapping, % */
	float rate_displacement;	/* SR after displacement, % */
	float rate_final;	/* SR after the deadline repair (final), % */
	int deadline_repairs;	/* moves of the deadline repair */
	int emptied_cores;	/* cores emptied by the consolidation */
	float swapping_gain;	/* SR points won by swapping */
	float displacement_gain;	/* SR points won by displacement */
	float placement_gain;	/* both */

	float fragmentation_rate;	/* FR: cuts / maximum cuts, % */
	float let_utilization;	/* sum of the LET loads of the used cores */
	float application_utilization;	/* sum of the application loads */
	float unused_utilization;	/* room left below phi */
	float system_capacity;	/* used cores x phi */

	/*
	 * critical WCET scaling factor of the mapping: how much every WCET can
	 * grow before one of the schedulable cores misses a deadline (the
	 * smallest factor of these cores); -1 if there is none
	 */
	float scaling_factor;
	struct mp_controller_stats controllers[MP_MAX_ECUS];
};

/*
 * mp_verify_mapping() - consistency checks of a finished mapping
 *
 * Recomputes every load (none above phi) and checks that on each core the
 * tasks of one chain have priorities in precedence order.
 * Return: 0, -ERANGE (overload) or -EINVAL (precedence violated).
 */
int mp_verify_mapping(struct mp_core_array *cores);

/*
 * mp_cores_lower_bound() - a lower bound of the cores any mapping of
 * @fragments needs
 *
 * Every core offers phi of its own capacity, and a fragment takes at least
 * u_f(c) of a core of controller c (its utilization on the cheapest type
 * of core of c it may run on). The bound fragments of controller c need
 * k_c = ceil(U_c / phi) cores there (U_c the sum of their u_f(c)), and
 * every fragment needs its share of the capacity: M >= max(sum_c k_c,
 * ceil((sum_c U_c + sum_free min_c u_f(c)) / phi)). LET tasks,
 * schedulability, memory and replicas can only add cores.
 */
int mp_cores_lower_bound(const struct mp_params *params,
			 const struct mp_chain_array *fragments);

/* metrics of the mapping in @cores, after placement */
void mp_compute_stats(const struct mp_ctx *ctx,
		      const struct mp_core_array *cores,
		      struct mp_stats *stats);

#endif /* MAPPING_STATS_H */
