// SPDX-License-Identifier: Apache-2.0
/* Architecture and bin-packing assignment (see assign.h). */
#include <errno.h>
#include <string.h>

#include "mapping/assign.h"
#include "mapping/array.h"
#include "mapping/let.h"
#include "mapping/sched.h"

/* the LET chain of a new core: one task, no cost until tasks arrive */
static int make_let_chain(struct mp_ctx *ctx, struct mp_chain *let_chain)
{
	struct mp_task let_task;

	memset(let_chain, 0, sizeof(*let_chain));
	let_chain->id = ctx->next_chain_id;
	ctx->next_chain_id++;
	let_chain->controller = -1;
	let_chain->is_let = true;

	memset(&let_task, 0, sizeof(let_task));
	let_task.priority = 1;		/* highest priority */
	let_task.chain_id = let_chain->id;
	let_task.unique_id = -1;	/* not part of the application */
	let_task.is_let = true;
	return mp_task_array_push(&let_chain->tasks, &let_task);
}

int mp_add_core(struct mp_ctx *ctx, struct mp_core_array *cores,
		int controller)
{
	struct mp_ecu ecu;

	mp_ecu_get(&ctx->params, controller, &ecu);
	return mp_add_typed_core(ctx, cores, controller, ecu.added_type);
}

/*
 * the WCET of the LET task of @core: slower on a slower core, and its
 * copies wait for the shared memory like any other request
 */
static int let_wcet_on(const struct mp_params *params,
		       const struct mp_core *core)
{
	int64_t wcet = (params->let_wcet * MP_REFERENCE_SPEED +
			core->speed - 1) / core->speed;

	if (params->let_wcet && core->access_stall)
		wcet += ((int64_t)mp_default_accesses(params, params->let_wcet) *
			 core->access_stall + 999) / 1000;
	return (int)wcet;
}

int mp_core_set_stall(const struct mp_ctx *ctx, struct mp_core *core,
		      int stall)
{
	int i, err;

	if (core->access_stall == stall)
		return 0;
	core->access_stall = stall;
	core->let_wcet = let_wcet_on(&ctx->params, core);
	for (i = 0; i < core->chains.count; i++)
		if (!core->chains.items[i].is_let)
			mp_chain_convert_to_core(&core->chains.items[i], core);
	err = mp_let_update(core, mp_core_period_gcd(core));
	if (err)
		return err;
	mp_core_update_load(core);
	return mp_core_build_task_list(core, core->id);
}

int mp_contention_update(const struct mp_ctx *ctx,
			 struct mp_core_array *cores)
{
	int used[MP_MAX_ECUS] = {0}, i, err;

	if (!ctx->params.memory_latency)
		return 0;
	for (i = 0; i < cores->count; i++) {
		const struct mp_core *core = &cores->items[i];

		if (core->controller >= 0 && core->controller < MP_MAX_ECUS)
			used[core->controller] += mp_core_period_gcd(core) > 0;
	}
	for (i = 0; i < cores->count; i++) {
		struct mp_core *core = &cores->items[i];
		bool schedulable;
		int stall;

		if (core->controller < 0 || core->controller >= MP_MAX_ECUS)
			continue;
		stall = mp_access_stall(&ctx->params, used[core->controller]);
		/* fewer cores than assumed: the WCETs can only go down */
		if (stall >= core->access_stall)
			continue;
		err = mp_core_set_stall(ctx, core, stall);
		if (err)
			return err;
		if (!mp_core_period_gcd(core))
			continue;
		/* the same priorities, shorter WCETs: new response times */
		err = mp_response_times(&core->tasks, &schedulable);
		if (err)
			return err;
		mp_core_write_back_tasks(core);
		core->schedulable = schedulable;
		err = mp_core_build_task_list(core, i);
		if (err)
			return err;
	}
	return 0;
}

int mp_add_typed_core(struct mp_ctx *ctx, struct mp_core_array *cores,
		      int controller, int type)
{
	struct mp_core core;
	struct mp_chain let_chain;
	int err, others = 0, i;

	memset(&core, 0, sizeof(core));
	core.id = ctx->core_count;
	core.fill_limit = ctx->params.fill_limit;
	core.type = type;
	core.speed = mp_ecu_speed(&ctx->params, controller, type);
	if (core.speed <= 0)
		return -EINVAL;		/* the ECU has no core of this type */
	/*
	 * contention: until the mapping is known, every core of the
	 * controller (of the architecture, or as many as there are) may
	 * compete for its memory
	 */
	for (i = 0; i < cores->count; i++)
		others += cores->items[i].controller == controller;
	core.access_stall = mp_access_stall(&ctx->params,
		others + 1 > mp_competing_cores(&ctx->params, controller) ?
		others + 1 : mp_competing_cores(&ctx->params, controller));
	core.let_wcet = let_wcet_on(&ctx->params, &core);
	core.schedulable = true;
	core.load_remaining = core.fill_limit;
	core.controller = controller;

	err = make_let_chain(ctx, &let_chain);
	if (!err)
		err = mp_chain_array_push(&core.chains, &let_chain);
	if (!err)
		err = mp_core_array_push(cores, &core);
	mp_chain_free(&let_chain);
	mp_core_free(&core);
	if (err)
		return err;

	ctx->core_count++;
	return 0;
}

int mp_architecture_init(struct mp_ctx *ctx, struct mp_core_array *cores)
{
	const struct mp_params *params = &ctx->params;
	int role, round, c, t, err;

	mp_core_array_free(cores);
	ctx->core_count = 0;
	/* LET chains are numbered after the chains of the application */
	ctx->next_chain_id = params->chain_count;

	/*
	 * role by role (central computers first), and inside a role one core
	 * of each ECU at a time; the cores of an ECU by type in type order
	 */
	for (role = 0; role < MP_ROLE_COUNT; role++) {
		int rounds = 0;

		for (c = 0; c < mp_ecu_count(params); c++)
			if ((int)mp_ecu_role(params, c) == role &&
			    mp_ecu_initial_cores(params, c) > rounds)
				rounds = mp_ecu_initial_cores(params, c);
		for (round = 0; round < rounds; round++)
			for (c = 0; c < mp_ecu_count(params); c++) {
				struct mp_ecu ecu;
				int below = 0;

				if ((int)mp_ecu_role(params, c) != role)
					continue;
				mp_ecu_get(params, c, &ecu);
				for (t = 0; t < MP_CORE_TYPES; t++) {
					if (round < below + ecu.cores[t])
						break;
					below += ecu.cores[t];
				}
				if (t == MP_CORE_TYPES)
					continue;	/* no core this round */
				err = mp_add_typed_core(ctx, cores, c, t);
				if (err)
					return err;
			}
	}
	ctx->architecture_cores = cores->count;
	return 0;
}

static bool same_fragment(const struct mp_chain *a, const struct mp_chain *b)
{
	return b && !a->is_let && !b->is_let && a->id == b->id &&
	       a->fragment_index == b->fragment_index;
}

int mp_controller_memory(const struct mp_core_array *cores, int controller)
{
	int memory = 0, i, j;

	for (i = 0; i < cores->count; i++)
		if (cores->items[i].controller == controller)
			for (j = 0; j < cores->items[i].chains.count; j++)
				memory += cores->items[i].chains.items[j].memory;
	return memory;
}

bool mp_placement_allowed(const struct mp_ctx *ctx,
			  const struct mp_core_array *cores,
			  const struct mp_chain *fragment, int target,
			  const struct mp_chain *leaving)
{
	const struct mp_core *core = &cores->items[target];
	int capacity = mp_ecu_memory(&ctx->params, core->controller);
	int i, j;

	if (!(mp_chain_allowed(fragment) & MP_CONTROLLER_BIT(core->controller)) ||
	    !(mp_chain_types(fragment) & (1u << core->type)))
		return false;
	/* practice baseline: the free software on the central computers */
	if (ctx->params.central_only && fragment->controller == MP_CONTROLLER_ANY &&
	    !mp_ecu_is_central(&ctx->params, core->controller) &&
	    mp_central_allowed(&ctx->params, mp_chain_allowed(fragment),
			       mp_chain_types(fragment)))
		return false;

	/* memory of the controller, without the fragment and @leaving */
	if (capacity > 0 && fragment->memory > 0) {
		/* a free fragment leaves room for the bound ones to come */
		int used = fragment->controller == MP_CONTROLLER_ANY ?
			   ctx->reserved_memory[core->controller] : 0;

		for (i = 0; i < cores->count; i++) {
			if (cores->items[i].controller != core->controller)
				continue;
			for (j = 0; j < cores->items[i].chains.count; j++) {
				const struct mp_chain *chain =
					&cores->items[i].chains.items[j];

				if (!same_fragment(chain, fragment) &&
				    !same_fragment(chain, leaving))
					used += chain->memory;
			}
		}
		if (used + fragment->memory > capacity)
			return false;
	}

	/* replicas apart */
	if (fragment->replica_group > 0)
		for (i = 0; i < cores->count; i++) {
			const struct mp_core *other = &cores->items[i];

			if (ctx->params.separation == MP_SEPARATE_CORES ?
			    i != target : other->controller != core->controller)
				continue;
			for (j = 0; j < other->chains.count; j++) {
				const struct mp_chain *chain = &other->chains.items[j];

				if (!chain->is_let &&
				    chain->replica_group == fragment->replica_group &&
				    chain->id != fragment->id &&
				    !same_fragment(chain, leaving))
					return false;
			}
		}
	return true;
}

/* the cores tried for a fragment, in turn, until one fits */
enum tier {
	TIER_SIBLINGS,	/* cores running a fragment of the same chain */
	TIER_PREFERRED,	/* its controller; a central computer for a free
			 * fragment */
	TIER_ANY,	/* every core of its controller, any for a free one */
};

/* whether @core holds a fragment of the original chain of @chain */
static bool holds_sibling(const struct mp_core *core,
			  const struct mp_chain *chain)
{
	int i;

	for (i = 0; i < core->chains.count; i++)
		if (!core->chains.items[i].is_let &&
		    core->chains.items[i].id == chain->id)
			return true;
	return false;
}

/*
 * The core of @cores that receives @chain under @algorithm, or -1 if none
 * can take it within its filling limit. @period_gcd gets the core's new
 * task period GCD (the new LET period).
 *
 *   BFDU: the core left with the least room (first one on ties);
 *   WFDU: the core left with the most room;
 *   FFDU: the first core where the chain fits.
 *
 * Only cores of the chain's controller are considered, unless the chain is
 * free (MP_CONTROLLER_ANY): then the cores of the central computers and
 * the other cores of the initial architecture (ids below
 * @architecture_cores). @tier narrows them further.
 */
/*
 * Where the neighbours of a fragment (the fragments before and after it
 * in its chain) are: a core index once placed, else the controller they
 * are bound to, else unknown (-1 both).
 */
struct neighbours {
	int core[2];
	int controller[2];
};

/* hops that placing the fragment on @core would create: over the network
 * (other controller) and inside a controller (other core) */
static void hops_created(const struct mp_core_array *cores,
			 const struct neighbours *n, int core,
			 int *network, int *local)
{
	int side, controller = cores->items[core].controller;

	*network = 0;
	*local = 0;
	for (side = 0; side < 2; side++) {
		if (n->core[side] >= 0) {
			if (n->core[side] == core)
				continue;
			if (cores->items[n->core[side]].controller != controller)
				(*network)++;
			else
				(*local)++;
		} else if (n->controller[side] >= 0 &&
			   n->controller[side] != controller) {
			(*network)++;
		}
	}
}

static int choose_core(const struct mp_ctx *ctx,
		       const struct mp_core_array *cores,
		       const struct mp_chain *chain, enum tier tier,
		       const struct neighbours *n, int *period_gcd)
{
	int chosen_network = 0, chosen_local = 0;
	bool chosen_pcu = false;
	enum mp_algorithm algorithm = ctx->params.algorithm;
	/* a central computer may take it (and has a core type it allows) */
	bool pcu_allowed = mp_host_allowed(&ctx->params,
					      mp_chain_allowed(chain),
					      mp_chain_types(chain));
	bool free = chain->controller == MP_CONTROLLER_ANY;
	int chosen = -1, chosen_room = 0, i;

	for (i = 0; i < cores->count; i++) {
		const struct mp_core *core = &cores->items[i];
		int new_load, room, gcd;
		bool better;

		if (!mp_placement_allowed(ctx, cores, chain, i, NULL))
			continue;
		if (tier == TIER_SIBLINGS && !holds_sibling(core, chain))
			continue;
		/*
		 * a core added to another ECU than a central computer only
		 * serves that ECU: free fragments there would force more
		 * cores for the bound ones
		 */
		if (free && pcu_allowed &&
		    !mp_ecu_is_host(&ctx->params, core->controller) &&
		    core->id >= ctx->architecture_cores)
			continue;
		/*
		 * free fragments go to the central computers first (CADU
		 * weighs them against the hops instead)
		 */
		if (tier == TIER_PREFERRED && free && pcu_allowed &&
		    algorithm != MP_CADU &&
		    !mp_ecu_is_host(&ctx->params, core->controller))
			continue;

		new_load = mp_core_load_with_chain(core, chain, &gcd);
		if (new_load > core->fill_limit)
			continue;

		room = core->fill_limit - new_load;
		if (algorithm == MP_CADU) {
			int network, local;
			bool pcu = mp_ecu_is_host(&ctx->params,
						     core->controller);

			/* fewest hops, then a central computer, then the
			 * least room */
			hops_created(cores, n, i, &network, &local);
			better = chosen < 0 || network < chosen_network ||
				 (network == chosen_network &&
				  (local < chosen_local ||
				   (local == chosen_local &&
				    (pcu > chosen_pcu ||
				     (pcu == chosen_pcu &&
				      room < chosen_room)))));
			if (better) {
				chosen_network = network;
				chosen_local = local;
				chosen_pcu = pcu;
			}
		} else switch (algorithm) {
		case MP_BFDU:
			better = chosen < 0 || room < chosen_room;
			break;
		case MP_WFDU:
			better = chosen < 0 || room > chosen_room;
			break;
		default:	/* MP_FFDU */
			better = true;
			break;
		}
		if (!better)
			continue;

		chosen = i;
		chosen_room = room;
		*period_gcd = gcd;
		if (algorithm == MP_FFDU)
			break;
	}
	return chosen;
}

int mp_core_add_chain(struct mp_core *core, const struct mp_chain *chain,
		      int period_gcd)
{
	int err;

	struct mp_chain *added;

	err = mp_chain_array_push(&core->chains, chain);
	if (err)
		return err;
	added = &core->chains.items[core->chains.count - 1];
	added->is_assigned = true;
	/* its WCETs and utilizations become those of this core */
	mp_chain_convert_to_core(added, core);
	err = mp_let_update(core, period_gcd);
	if (err)
		return err;
	mp_core_update_load(core);
	return mp_core_build_task_list(core, core->id);
}

int mp_core_remove_chain(struct mp_core *core, int chain_id,
			 int fragment_index)
{
	int i;

	for (i = 0; i < core->chains.count; i++) {
		struct mp_chain *chain = &core->chains.items[i];

		/* a LET chain id may equal an application chain id */
		if (chain->is_let || chain->id != chain_id ||
		    chain->fragment_index != fragment_index)
			continue;
		mp_chain_free(chain);
		mp_array_remove(core->chains.items, &core->chains.count, i,
				sizeof(*chain));
		mp_task_array_free(&core->tasks);
		/* no task left: GCD 0, the LET task costs nothing */
		mp_let_update(core, mp_core_period_gcd(core));
		mp_core_update_load(core);
		return 0;
	}
	return -ENOENT;
}

/* final consistency check: loads recomputed and within the limits */
static int verify_core_loads(struct mp_core_array *cores)
{
	int i;

	for (i = 0; i < cores->count; i++) {
		mp_core_update_load(&cores->items[i]);
		if (cores->items[i].load_remaining < 0)
			return -ERANGE;
	}
	return 0;
}

int mp_best_core_type(const struct mp_params *params,
		      const struct mp_chain *chain, int controller)
{
	unsigned allowed = mp_chain_types(chain) &
			   mp_ecu_types(params, controller);
	int best = -1, best_load = 0, type;

	for (type = 0; type < MP_CORE_TYPES; type++) {
		int load;

		if (!(allowed & (1u << type)))
			continue;
		load = mp_chain_utilization_on(chain,
					       mp_ecu_speed(params, controller,
							    type), type);
		if (best < 0 || load < best_load) {
			best = type;
			best_load = load;
		}
	}
	return best;
}

/*
 * the type of a new core for @chain on @controller: the added type of the
 * ECU if @chain may run on it, else the allowed type on which @chain is
 * lightest
 */
static int added_core_type(const struct mp_params *params,
			   const struct mp_chain *chain, int controller)
{
	struct mp_ecu ecu;

	mp_ecu_get(params, controller, &ecu);
	if (mp_chain_types(chain) & (1u << ecu.added_type))
		return ecu.added_type;
	return mp_best_core_type(params, chain, controller);
}

/*
 * The controller of a new core for @chain: its own for a bound fragment;
 * for a free one a central computer if its constraints allow, else the
 * first other ECU that does (a new core there would hold no replica of
 * it, its memory must suffice and it must have room for one more core).
 * -1 if none.
 */

static int new_core_controller(const struct mp_ctx *ctx,
			       const struct mp_core_array *cores,
			       const struct mp_chain *chain)
{
	unsigned allowed = mp_chain_allowed(chain);
	int order[MP_MAX_ECUS], count, controller, n, i, j;

	count = mp_ecu_preference(&ctx->params, order);
	for (n = 0; n < count; n++) {
		int capacity, max_cores, have = 0;
		bool replica_there = false;

		controller = order[n];
		if (mp_best_core_type(&ctx->params, chain, controller) < 0)
			continue;
		capacity = mp_ecu_memory(&ctx->params, controller);
		if (!(allowed & MP_CONTROLLER_BIT(controller)))
			continue;
		max_cores = mp_ecu_max_cores(&ctx->params, controller);
		for (i = 0; i < cores->count; i++)
			have += cores->items[i].controller == controller;
		if (max_cores && have >= max_cores)
			continue;
		if (capacity > 0 && mp_controller_memory(cores, controller) +
		    chain->memory + (chain->controller == MP_CONTROLLER_ANY ?
				     ctx->reserved_memory[controller] : 0) >
		    capacity)
			continue;
		if (chain->replica_group > 0 &&
		    ctx->params.separation == MP_SEPARATE_CONTROLLERS)
			for (i = 0; i < cores->count; i++) {
				if (cores->items[i].controller != controller)
					continue;
				for (j = 0; j < cores->items[i].chains.count; j++) {
					const struct mp_chain *other =
						&cores->items[i].chains.items[j];

					if (!other->is_let &&
					    other->replica_group ==
					    chain->replica_group &&
					    other->id != chain->id)
						replica_there = true;
				}
			}
		if (!replica_there)
			return controller;
	}
	return -1;
}

/*
 * Place @chain (not assigned yet) on a core of @cores, adding a core if
 * none can take it. *@placed is false if not even a new core can.
 */
/* the neighbours of @chain among @chains, as placed on @cores so far */
static void find_neighbours(const struct mp_chain_array *chains,
			    const struct mp_core_array *cores,
			    const struct mp_chain *chain, struct neighbours *n)
{
	int side, i, j;

	for (side = 0; side < 2; side++) {
		int index = chain->fragment_index + (side ? 1 : -1);

		n->core[side] = -1;
		n->controller[side] = -1;
		for (i = 0; i < chains->count; i++) {
			const struct mp_chain *other = &chains->items[i];

			if (other->id != chain->id ||
			    other->fragment_index != index)
				continue;
			if (other->controller != MP_CONTROLLER_ANY)
				n->controller[side] = other->controller;
			if (!other->is_assigned)
				break;
			for (j = 0; j < cores->count && n->core[side] < 0; j++) {
				const struct mp_core *core = &cores->items[j];
				int k;

				for (k = 0; k < core->chains.count; k++)
					if (!core->chains.items[k].is_let &&
					    core->chains.items[k].id == chain->id &&
					    core->chains.items[k].fragment_index ==
					    index)
						n->core[side] = j;
			}
			break;
		}
	}
}

static int place_fragment(struct mp_ctx *ctx,
			  const struct mp_chain_array *chains,
			  struct mp_chain *chain, struct mp_core_array *cores,
			  bool *placed)
{
	int core_index = -1, period_gcd = 0, controller, attempt, err;
	struct neighbours n;

	*placed = false;
	memset(&n, 0xff, sizeof(n));	/* all unknown (-1) */
	if (ctx->params.algorithm == MP_CADU)
		find_neighbours(chains, cores, chain, &n);
	for (attempt = 0; attempt < 2; attempt++) {
		/*
		 * affinity: a core that already runs part of the chain saves
		 * a hop, so it is tried first
		 */
		if (ctx->params.affinity)
			core_index = choose_core(ctx, cores, chain, TIER_SIBLINGS, &n,
						 &period_gcd);
		if (core_index < 0)
			core_index = choose_core(ctx, cores, chain, TIER_PREFERRED, &n,
						 &period_gcd);
		if (core_index < 0)
			core_index = choose_core(ctx, cores, chain, TIER_ANY, &n,
						 &period_gcd);
		if (core_index >= 0) {
			err = mp_core_add_chain(&cores->items[core_index],
						chain, period_gcd);
			if (err)
				return err;
			chain->is_assigned = true;
			*placed = true;
			return 0;
		}
		if (attempt || ctx->params.fixed_hardware)
			break;	/* not even an empty core can take it */

		/*
		 * a new core on the controller of the fragment; a free
		 * fragment goes to the central computer
		 */
		controller = new_core_controller(ctx, cores, chain);
		if (controller < 0)
			break;		/* no controller can take it */
		err = mp_add_typed_core(ctx, cores, controller,
					added_core_type(&ctx->params, chain,
							controller));
		if (err)
			return err;
	}
	return 0;
}

int mp_assign(struct mp_ctx *ctx, struct mp_chain_array *chains,
	      struct mp_core_array *cores)
{
	int i, err = 0;

	ctx->feasible = true;
	/* the memory of the bound fragments is reserved on their controller */
	memset(ctx->reserved_memory, 0, sizeof(ctx->reserved_memory));
	for (i = 0; i < chains->count; i++)
		if (!chains->items[i].is_assigned &&
		    chains->items[i].controller >= 0 &&
		    chains->items[i].controller < MP_MAX_ECUS)
			ctx->reserved_memory[chains->items[i].controller] +=
				chains->items[i].memory;
	/* fragments in decreasing utilization order (partitioning output) */
	for (i = 0; i < chains->count; i++) {
		struct mp_chain *chain = &chains->items[i];
		bool placed;

		if (chain->is_assigned)
			continue;
		err = place_fragment(ctx, chains, chain, cores, &placed);
		if (err)
			break;
		if (!placed) {
			ctx->feasible = false;
			ctx->unplaced_chain = chain->id;
			ctx->unplaced_fragment = chain->fragment_index;
			break;
		}
		if (chain->controller >= 0 &&
		    chain->controller < MP_MAX_ECUS)
			ctx->reserved_memory[chain->controller] -= chain->memory;
	}
	memset(ctx->reserved_memory, 0, sizeof(ctx->reserved_memory));
	if (err || !ctx->feasible)
		return err;
	return verify_core_loads(cores);
}
