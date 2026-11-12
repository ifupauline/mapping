/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MAPPING_PLACEMENT_H
#define MAPPING_PLACEMENT_H

#include "mapping/context.h"
#include "mapping/model.h"

/*
 * Placement phase: fragments of unschedulable cores are moved to make
 * those cores schedulable, without adding cores.
 *
 * Swapping exchanges two fragments of two unschedulable cores (same
 * controller, unless both fragments are free) when the exchange fits
 * both cores and makes them schedulable (both, if one already was; at
 * least one, if neither was).
 *
 * Displacement moves a fragment of an unschedulable core to the schedulable
 * core, of the right controller, that remains schedulable and keeps the
 * most room; cores without application tasks are only tried when no other
 * core can take the fragment.
 */

int mp_swapping(struct mp_ctx *ctx, struct mp_core_array *cores);

/*
 * mp_consolidation() - empty cores after the assignment
 *
 * The cores in use are taken by increasing load; the fragments of a core
 * are moved, largest first, each to another core in use that is allowed
 * (controller, type, memory, replicas; a free fragment does not go to a
 * zone core added to the architecture), stays within its filling limit
 * and stays schedulable (optimal priority assignment): the one that
 * leaves the fewest network hops to the neighbours of the fragment in its
 * chain, then the fullest. If every
 * fragment of the core finds a place, the moves are kept and the core is
 * empty: one core fewer; otherwise nothing changes. Repeated until no core
 * can be emptied. If ctx->params.consolidate, for every rule but WFDU,
 * which spreads the load on purpose.
 * @emptied: set to the number of cores emptied (may be NULL)
 * Return: 0 or a negative errno.
 */
int mp_consolidation(struct mp_ctx *ctx, struct mp_core_array *cores,
		     int *emptied);
int mp_displacement(struct mp_ctx *ctx, struct mp_core_array *cores);

/* index in @cores of the core with id @core_id, or -1 */
int mp_find_core(const struct mp_core_array *cores, int core_id);

#endif /* MAPPING_PLACEMENT_H */
