// SPDX-License-Identifier: Apache-2.0
/* Placement phase: swapping and displacement (see placement.h). */
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "mapping/array.h"
#include "mapping/assign.h"
#include "mapping/let.h"
#include "mapping/placement.h"
#include "mapping/sched.h"

/* a fragment taken from an unschedulable core, and where it is now */
struct stored_chain {
	struct mp_chain chain;	/* copy made when the list is built */
	int core_id;		/* id of the core holding the fragment */
};

struct stored_chains {
	struct stored_chain *items;
	int count;
	int capacity;
};

static void free_stored_chains(struct stored_chains *stored)
{
	int i;

	for (i = 0; i < stored->count; i++)
		mp_chain_free(&stored->items[i].chain);
	free(stored->items);
	memset(stored, 0, sizeof(*stored));
}

/* every fragment (not the LET chains) of every unschedulable core */
static int store_chains_of_late_cores(const struct mp_core_array *cores,
				      struct stored_chains *stored)
{
	int i, j, err;

	memset(stored, 0, sizeof(*stored));
	for (i = 0; i < cores->count; i++) {
		const struct mp_core *core = &cores->items[i];

		if (core->schedulable)
			continue;
		for (j = 0; j < core->chains.count; j++) {
			struct stored_chain item;

			if (core->chains.items[j].is_let)
				continue;
			err = mp_chain_copy(&item.chain, &core->chains.items[j]);
			if (err)
				goto fail;
			mp_chain_update_utilization(&item.chain);
			item.core_id = core->id;
			err = mp_array_insert((void **)&stored->items,
					      &stored->count, &stored->capacity,
					      stored->count, &item, sizeof(item));
			if (err) {
				mp_chain_free(&item.chain);
				goto fail;
			}
		}
	}
	return 0;
fail:
	free_stored_chains(stored);
	return err;
}

int mp_find_core(const struct mp_core_array *cores, int core_id)
{
	int i;

	for (i = 0; i < cores->count; i++)
		if (cores->items[i].id == core_id)
			return i;
	return -1;
}

/* a core is empty when it runs no application task (its load is 0) */
static void update_empty_flags(struct mp_core_array *cores)
{
	int i;

	for (i = 0; i < cores->count; i++)
		cores->items[i].is_empty = cores->items[i].load == 0;
}

/* recompute every load; a core above its filling limit is a bug */
static int verify_core_loads(struct mp_core_array *cores)
{
	int i;

	for (i = 0; i < cores->count; i++)
		if (mp_core_update_load(&cores->items[i]) >
		    cores->items[i].fill_limit)
			return -ERANGE;
	return 0;
}

/* replace the core with the id of @replacement by it (moved, not copied) */
static void replace_core(struct mp_core_array *cores,
			 struct mp_core *replacement)
{
	int index = mp_find_core(cores, replacement->id);

	mp_core_free(&cores->items[index]);
	cores->items[index] = *replacement;
	memset(replacement, 0, sizeof(*replacement));
}

/* ---- swapping -------------------------------------------------------- */

/*
 * Whether fragments @first and @second may be exchanged: never on the same
 * core, and each must be allowed on the other's core once the other has
 * left it (controller, memory, replicas).
 */
static bool swap_allowed(const struct mp_ctx *ctx,
			 const struct mp_core_array *cores,
			 const struct stored_chain *first,
			 const struct stored_chain *second)
{
	int first_core = mp_find_core(cores, first->core_id);
	int second_core = mp_find_core(cores, second->core_id);

	if (first->core_id == second->core_id)
		return false;
	return mp_placement_allowed(ctx, cores, &first->chain, second_core,
				    &second->chain) &&
	       mp_placement_allowed(ctx, cores, &second->chain, first_core,
				    &first->chain);
}

static const struct mp_core *core_of(const struct mp_core_array *cores,
				     const struct stored_chain *item)
{
	return &cores->items[mp_find_core(cores, item->core_id)];
}

/*
 * Whether @incoming fits within the filling limit of @core in place of
 * @outgoing: the load of the other chains, plus @incoming, plus the LET
 * task resized for the new period GCD (what removing @outgoing and then
 * mp_core_load_with_chain() would give, without copying the core).
 */
static bool fits_in_place(const struct mp_core *core,
			  const struct mp_chain *outgoing,
			  const struct mp_chain *incoming)
{
	int load = mp_chain_utilization_on_core(incoming, core), gcd = 0;
	int i, j;

	for (j = 0; j < incoming->tasks.count; j++)
		gcd = mp_gcd(gcd, incoming->tasks.items[j].period);
	for (i = 0; i < core->chains.count; i++) {
		const struct mp_chain *chain = &core->chains.items[i];

		if (chain->is_let || (chain->id == outgoing->id &&
				      chain->fragment_index ==
				      outgoing->fragment_index))
			continue;
		load += chain->utilization;
		for (j = 0; j < chain->tasks.count; j++)
			gcd = mp_gcd(gcd, chain->tasks.items[j].period);
	}
	load += gcd ? mp_let_utilization(core->let_wcet, gcd) : 0;
	return load <= core->fill_limit;
}

/*
 * Half of an exchange: on a copy @trial of the core holding @outgoing,
 * replace @outgoing by @incoming and assign priorities, if it fits.
 */
static int try_half_exchange(struct mp_ctx *ctx,
			     const struct mp_core_array *cores,
			     const struct stored_chain *outgoing,
			     const struct mp_chain *incoming,
			     struct mp_core *trial, bool *was_schedulable,
			     bool *fits)
{
	const struct mp_core *core;
	int load, gcd, err;

	core = &cores->items[mp_find_core(cores, outgoing->core_id)];
	*was_schedulable = core->schedulable;
	err = mp_core_copy(trial, core);
	if (err)
		return err;
	err = mp_core_remove_chain(trial, outgoing->chain.id,
				   outgoing->chain.fragment_index);
	if (err)
		return err;

	load = mp_core_load_with_chain(trial, incoming, &gcd);
	*fits = load <= trial->fill_limit;
	if (!*fits)
		return 0;
	err = mp_core_add_chain(trial, incoming, gcd);
	if (err)
		return err;
	return mp_assign_priorities(ctx, trial);
}

/*
 * Try to exchange @source and @target; on success the two cores are
 * replaced by their new versions and the stored core ids updated.
 */
static int try_exchange(struct mp_ctx *ctx, struct mp_core_array *cores,
			struct stored_chain *source,
			struct stored_chain *target)
{
	struct mp_core target_core = {0}, source_core = {0};
	bool target_was_ok, source_was_ok, target_fits, source_fits;
	bool accepted = false;
	int err;

	/*
	 * Cheap tests first (they do not change the outcome, only skip the
	 * priority assignments of exchanges that cannot be accepted): two
	 * schedulable cores have nothing to gain, and both fragments must fit
	 * in place of each other.
	 */
	if (core_of(cores, target)->schedulable &&
	    core_of(cores, source)->schedulable)
		return 0;
	if (!fits_in_place(core_of(cores, target), &target->chain,
			   &source->chain) ||
	    !fits_in_place(core_of(cores, source), &source->chain,
			   &target->chain))
		return 0;

	/* the target core receives the source fragment, and back */
	err = try_half_exchange(ctx, cores, target, &source->chain,
				&target_core, &target_was_ok, &target_fits);
	if (!err)
		err = try_half_exchange(ctx, cores, source, &target->chain,
					&source_core, &source_was_ok,
					&source_fits);
	if (err)
		goto out;

	if (target_was_ok && source_was_ok)
		goto out;		/* nothing to gain */
	if (target_fits && source_fits) {
		if (target_was_ok || source_was_ok)
			/* one core was fine: both must be fine after */
			accepted = target_core.schedulable &&
				   source_core.schedulable;
		else
			/* neither was: one fixed core is progress */
			accepted = target_core.schedulable ||
				   source_core.schedulable;
	}
	if (!accepted)
		goto out;

	target->core_id = source_core.id;
	source->core_id = target_core.id;
	replace_core(cores, &target_core);
	replace_core(cores, &source_core);
out:
	mp_core_free(&target_core);
	mp_core_free(&source_core);
	return err;
}

int mp_swapping(struct mp_ctx *ctx, struct mp_core_array *cores)
{
	struct stored_chains stored;
	int i, j, err;

	update_empty_flags(cores);
	err = store_chains_of_late_cores(cores, &stored);
	if (err || !stored.count)
		return err;

	/* every pair of fragments once (an exchange is symmetric) */
	for (i = 0; i < stored.count; i++)
		for (j = i + 1; j < stored.count; j++) {
			if (!swap_allowed(ctx, cores, &stored.items[i],
					  &stored.items[j]))
				continue;
			err = try_exchange(ctx, cores, &stored.items[i],
					   &stored.items[j]);
			if (err)
				goto out;
		}

	update_empty_flags(cores);
	err = verify_core_loads(cores);
out:
	free_stored_chains(&stored);
	return err;
}

/* ---- displacement ---------------------------------------------------- */

/* rebuild the task list of every core (positions refer to @cores) */
static int rebuild_task_lists(struct mp_core_array *cores)
{
	int i, err;

	for (i = 0; i < cores->count; i++) {
		err = mp_core_build_task_list(&cores->items[i], i);
		if (err)
			return err;
	}
	return 0;
}

/*
 * The cores that could receive @item: allowed (controller, memory,
 * replicas), schedulable, not its own core, room for it, and not empty
 * unless @allow_empty. Each
 * candidate is a copy of the core with the fragment added.
 */
static int collect_candidates(const struct mp_ctx *ctx,
			      const struct mp_core_array *cores,
			      const struct stored_chain *item,
			      bool allow_empty,
			      struct mp_core_array *candidates)
{
	int i, load, gcd, err;

	memset(candidates, 0, sizeof(*candidates));
	for (i = 0; i < cores->count; i++) {
		const struct mp_core *core = &cores->items[i];

		if (!mp_placement_allowed(ctx, cores, &item->chain, i, NULL))
			continue;
		if (!core->schedulable || core->id == item->core_id)
			continue;
		if (core->is_empty && !allow_empty)
			continue;

		load = mp_core_load_with_chain(core, &item->chain, &gcd);
		if (load > core->fill_limit)
			continue;
		err = mp_core_array_push(candidates, core);
		if (!err)
			err = mp_core_add_chain(&candidates->items[
						candidates->count - 1],
						&item->chain, gcd);
		if (err)
			return err;
	}
	return 0;
}

/*
 * Among @candidates, the one that stays schedulable with the most room
 * left (first one on ties), or -1.
 */
static int best_candidate(struct mp_ctx *ctx,
			  struct mp_core_array *candidates)
{
	int best = -1, i, err;

	err = rebuild_task_lists(candidates);
	if (err)
		return err;
	for (i = 0; i < candidates->count; i++) {
		struct mp_core *candidate = &candidates->items[i];

		err = mp_assign_priorities(ctx, candidate);
		if (err)
			return err;
		if (!candidate->schedulable)
			continue;
		if (best < 0 || candidate->load_remaining >
				candidates->items[best].load_remaining)
			best = i;
	}
	return best;
}

/* move @item from its core to @destination (moved into @cores) */
static int displace(struct mp_ctx *ctx, struct mp_core_array *cores,
		    struct stored_chain *item, struct mp_core *destination)
{
	int source, target, err;

	source = mp_find_core(cores, item->core_id);
	err = mp_core_remove_chain(&cores->items[source], item->chain.id,
				   item->chain.fragment_index);
	if (!err)
		err = rebuild_task_lists(cores);
	if (!err)
		err = mp_assign_priorities(ctx, &cores->items[source]);
	if (err)
		return err;

	item->core_id = destination->id;
	replace_core(cores, destination);
	target = mp_find_core(cores, item->core_id);
	err = rebuild_task_lists(cores);
	if (!err)
		err = mp_assign_priorities(ctx, &cores->items[target]);
	if (err)
		return err;
	/* it was schedulable as a candidate: it must still be */
	return cores->items[target].schedulable ? 0 : -EINVAL;
}

int mp_displacement(struct mp_ctx *ctx, struct mp_core_array *cores)
{
	struct mp_core_array candidates = {0};
	struct stored_chains stored;
	bool allow_empty = false;	/* second try for the same fragment */
	int i, best, err;

	update_empty_flags(cores);
	err = store_chains_of_late_cores(cores, &stored);
	if (err || !stored.count)
		return err;

	for (i = 0; i < stored.count; i++) {
		struct stored_chain *item = &stored.items[i];

		/* its core was fixed by an earlier displacement */
		if (cores->items[mp_find_core(cores, item->core_id)].schedulable) {
			allow_empty = false;
			continue;
		}

		err = collect_candidates(ctx, cores, item, allow_empty,
					 &candidates);
		if (err)
			goto out;
		best = best_candidate(ctx, &candidates);
		if (best < -1) {
			err = best;
			goto out;
		}

		if (best >= 0) {
			err = displace(ctx, cores, item,
				       &candidates.items[best]);
			if (err)
				goto out;
			allow_empty = false;
		} else if (!allow_empty) {
			/* retry the same fragment, empty cores allowed */
			allow_empty = true;
			i--;
		} else {
			allow_empty = false;
		}
		mp_core_array_free(&candidates);
	}

	update_empty_flags(cores);
	err = verify_core_loads(cores);
out:
	mp_core_array_free(&candidates);
	free_stored_chains(&stored);
	return err;
}

/* ---- consolidation ----------------------------------------------------- */

static bool runs_tasks(const struct mp_core *core)
{
	return mp_core_period_gcd(core) > 0;
}

/*
 * a candidate core for a fragment: the network hops the move would leave
 * between the fragment and its neighbours in the chain, the room left
 */
struct target {
	int index;
	int hops;
	int room;
	int gcd;
};

static bool target_less(const void *a, const void *b)
{
	const struct target *x = a, *y = b;

	return x->hops != y->hops ? x->hops < y->hops : x->room < y->room;
}

/* the controllers of the neighbours of @fragment in its chain (-1: none) */
static void neighbour_controllers(const struct mp_core_array *cores,
				  const struct mp_chain *fragment, int *where)
{
	int i, j, side;

	where[0] = where[1] = -1;
	for (i = 0; i < cores->count; i++)
		for (j = 0; j < cores->items[i].chains.count; j++) {
			const struct mp_chain *c = &cores->items[i].chains.items[j];

			if (c->is_let || c->id != fragment->id)
				continue;
			for (side = 0; side < 2; side++)
				if (c->fragment_index == fragment->fragment_index +
				    (side ? 1 : -1))
					where[side] = cores->items[i].controller;
		}
}

static bool larger_chain(const void *a, const void *b)
{
	return ((const struct mp_chain *)a)->utilization >
	       ((const struct mp_chain *)b)->utilization;
}

/*
 * @fragment (on core @source of @work) to the other core in use that
 * takes it and stays schedulable: the fewest network hops to its
 * neighbours in the chain, then the fullest; false if none
 */
static int move_out(const struct mp_ctx *ctx, struct mp_core_array *work,
		    int source, const struct mp_chain *fragment, bool *moved)
{
	bool pcu_allowed = mp_host_allowed(&ctx->params,
					      mp_chain_allowed(fragment),
					      mp_chain_types(fragment));
	bool unbound = fragment->controller == MP_CONTROLLER_ANY;
	struct target *targets;
	struct mp_ctx scratch = *ctx;
	int count = 0, i, err = 0, where[2];

	*moved = false;
	targets = malloc(((size_t)work->count + 1) * sizeof(*targets));
	if (!targets)
		return -ENOMEM;
	neighbour_controllers(work, fragment, where);
	for (i = 0; i < work->count; i++) {
		const struct mp_core *core = &work->items[i];
		int load, gcd;

		if (i == source || !runs_tasks(core))
			continue;
		/* as in the assignment: cores added to an ECU other than a
		 * central computer serve that ECU */
		if (unbound && pcu_allowed &&
		    !mp_ecu_is_host(&ctx->params, core->controller) &&
		    core->id >= ctx->architecture_cores)
			continue;
		if (!mp_placement_allowed(ctx, work, fragment, i, fragment))
			continue;
		load = mp_core_load_with_chain(core, fragment, &gcd);
		if (load > core->fill_limit)
			continue;
		targets[count].index = i;
		targets[count].hops = (where[0] >= 0 && where[0] != core->controller) +
				      (where[1] >= 0 && where[1] != core->controller);
		targets[count].room = core->fill_limit - load;
		targets[count].gcd = gcd;
		count++;
	}
	err = mp_stable_sort(targets, count, sizeof(*targets), target_less);
	for (i = 0; !err && i < count && !*moved; i++) {
		struct mp_core trial;

		err = mp_core_copy(&trial, &work->items[targets[i].index]);
		if (!err)
			err = mp_core_add_chain(&trial, fragment, targets[i].gcd);
		if (!err)
			err = mp_assign_priorities_first_fit(&scratch, &trial);
		if (!err && trial.schedulable) {
			mp_core_free(&work->items[targets[i].index]);
			work->items[targets[i].index] = trial;
			*moved = true;
			err = mp_core_remove_chain(&work->items[source],
						   fragment->id,
						   fragment->fragment_index);
		} else {
			mp_core_free(&trial);
		}
	}
	free(targets);
	return err;
}

/* every fragment of core @source elsewhere: @cores changes only if so */
static int try_empty(const struct mp_ctx *ctx, struct mp_core_array *cores,
		     int source, bool *emptied)
{
	struct mp_core_array work;
	struct mp_chain_array fragments = {0};
	int i, err;
	bool moved = true;

	*emptied = false;
	err = mp_core_array_copy(&work, cores);
	for (i = 0; !err && i < cores->items[source].chains.count; i++)
		if (!cores->items[source].chains.items[i].is_let)
			err = mp_chain_array_push(&fragments,
					&cores->items[source].chains.items[i]);
	if (!err)
		err = mp_stable_sort(fragments.items, fragments.count,
				     sizeof(*fragments.items), larger_chain);
	for (i = 0; !err && moved && i < fragments.count; i++)
		err = move_out(ctx, &work, source, &fragments.items[i], &moved);
	mp_chain_array_free(&fragments);
	if (!err && moved) {
		for (i = 0; !err && i < work.count; i++)
			err = mp_core_build_task_list(&work.items[i], i);
		if (!err) {
			mp_core_array_free(cores);
			*cores = work;
			*emptied = true;
			return 0;
		}
	}
	mp_core_array_free(&work);
	return err;
}

static bool lighter_core(const void *a, const void *b)
{
	return ((const struct target *)a)->room <
	       ((const struct target *)b)->room;
}

int mp_consolidation(struct mp_ctx *ctx, struct mp_core_array *cores,
		     int *emptied)
{
	struct target *order;
	bool progress = true;
	int count, i, err = 0;

	if (emptied)
		*emptied = 0;
	if (!ctx->params.consolidate || ctx->params.algorithm == MP_WFDU)
		return 0;
	order = malloc(((size_t)cores->count + 1) * sizeof(*order));
	if (!order)
		return -ENOMEM;
	while (progress && !err) {
		progress = false;
		/* the cores in use, lightest first (room as minus the load) */
		count = 0;
		for (i = 0; i < cores->count; i++)
			if (runs_tasks(&cores->items[i])) {
				order[count].index = i;
				order[count].room = cores->items[i].load;
				count++;
			}
		err = mp_stable_sort(order, count, sizeof(*order), lighter_core);
		for (i = 0; !err && i < count && !progress; i++)
			err = try_empty(ctx, cores, order[i].index, &progress);
		if (progress && emptied)
			(*emptied)++;
	}
	free(order);
	return err;
}
