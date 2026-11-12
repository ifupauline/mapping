/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MAPPING_ASSIGN_H
#define MAPPING_ASSIGN_H

#include "mapping/context.h"
#include "mapping/model.h"

/*
 * Architecture and assignment.
 *
 * The initial architecture has the initial cores of every ECU of the
 * platform (context.h), at the speed of their type. Assignment packs the
 * fragments, in decreasing utilization order, into the cores with a
 * bin-packing heuristic (BFDU, WFDU, FFDU or CADU), trying first the cores
 * that already run a fragment of the same chain (affinity), then, for a
 * free fragment, the cores of the central computers, then any core. A
 * fragment that fits nowhere gets a new core on its ECU (a central
 * computer first for a free fragment, up to the largest number of cores
 * of the ECU); if it does not even fit an empty core the design point is
 * unfeasible.
 */

/*
 * mp_add_core() - append an empty core to @cores
 * @controller: ECU of the new core; the core has the type of the cores
 *		the ECU adds, and its speed
 *
 * The core gets id ctx->core_count and a LET chain with id
 * ctx->next_chain_id. Return: 0 or -ENOMEM.
 */
int mp_add_core(struct mp_ctx *ctx, struct mp_core_array *cores,
		int controller);

/*
 * mp_architecture_init() - build the initial architecture in @cores
 * (emptied first) and reset the core and LET chain counters of @ctx. The
 * cores are numbered role by role (central computers first); inside a
 * role, one core of each ECU at a time, and the cores of an ECU by type.
 * Return: 0, -ENOMEM, or -EINVAL for a type the ECU does not have.
 */
int mp_architecture_init(struct mp_ctx *ctx, struct mp_core_array *cores);

/* mp_add_core() with a core of type @type (MP_CORE_BIG...); -EINVAL if
 * the ECU has no core of that type */
int mp_add_typed_core(struct mp_ctx *ctx, struct mp_core_array *cores,
		      int controller, int type);

/*
 * the type of core of @controller (among the types of the ECU, see
 * mp_ecu_types()) where @chain has the lowest utilization, among the
 * types it may run on (the first type on ties); -1 if none
 */
int mp_best_core_type(const struct mp_params *params,
		      const struct mp_chain *chain, int controller);

/*
 * mp_contention_update() - shared-resource contention of the final
 * mapping (nothing without a memory latency): only the cores that run
 * tasks compete for the memory of their controller, so the WCETs can only
 * go down; the response times are computed again with the same priorities
 * Return: 0 or a negative errno.
 */
int mp_contention_update(const struct mp_ctx *ctx,
			 struct mp_core_array *cores);

/*
 * mp_core_set_stall() - the contention of @core becomes @stall (ns per
 * memory request): its LET task and the tasks on it get their new WCETs,
 * its load and task list follow (the priorities are kept)
 * Return: 0 or a negative errno.
 */
int mp_core_set_stall(const struct mp_ctx *ctx, struct mp_core *core,
		      int stall);

/*
 * mp_core_add_chain() - put a copy of @chain on @core
 * @period_gcd: the task period GCD of the core with the chain (given by
 *		mp_core_load_with_chain()), the new LET period
 *
 * The LET task and the load are updated and the task list is rebuilt.
 * The caller has checked that the chain fits. Return: 0 or -ENOMEM.
 */
int mp_core_add_chain(struct mp_core *core, const struct mp_chain *chain,
		      int period_gcd);

/*
 * mp_core_remove_chain() - remove a fragment from @core
 * @chain_id, @fragment_index: identify the fragment (never the LET chain)
 *
 * The LET task is resized for the remaining tasks and the load updated;
 * the task list is emptied. Return: 0 or -ENOENT.
 */
int mp_core_remove_chain(struct mp_core *core, int chain_id,
			 int fragment_index);

/*
 * mp_assign() - assign every fragment of @chains to a core of @cores
 * @chains: the fragments, sorted by decreasing utilization (partitioning
 *	    output); is_assigned is set on each one placed
 *
 * Each fragment in turn goes to the first tier of cores where one can take
 * it (cores of its chain, then its controller or the PCU, then any allowed
 * core: a free fragment may use the zone cores of the initial
 * architecture), chosen by ctx->params.algorithm, or to a new core of its
 * controller (the PCU for a free fragment). Sets ctx->feasible to false
 * when a fragment cannot be placed even on an empty core (the cores are
 * then left as they were at that point).
 * Return: 0, or a negative errno on allocation failure or inconsistency.
 */
int mp_assign(struct mp_ctx *ctx, struct mp_chain_array *chains,
	      struct mp_core_array *cores);

/**
 * mp_placement_allowed() - whether @fragment may run on core @target
 * @leaving: a fragment that leaves @target at the same time (an exchange),
 *	     or NULL
 *
 * The spatial constraints, load aside: the controller of the core is
 * allowed for the fragment, the memory of that controller can hold it,
 * and no replica of its chain runs on the same controller (or core, with
 * MP_SEPARATE_CORES). The fragment itself, if already placed, is not
 * counted against itself.
 */
bool mp_placement_allowed(const struct mp_ctx *ctx,
			  const struct mp_core_array *cores,
			  const struct mp_chain *fragment, int target,
			  const struct mp_chain *leaving);

/* memory used on controller @controller, KB */
int mp_controller_memory(const struct mp_core_array *cores, int controller);

#endif /* MAPPING_ASSIGN_H */
