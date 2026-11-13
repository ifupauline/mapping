// SPDX-License-Identifier: Apache-2.0
/*
 * Deadline repair: a placement phase for the
 * task-chains whose end-to-end delay exceeds their end-to-end deadline.
 *
 * The same moves serve the chains crossing a port above the bandwidth
 * limit of the network (struct mp_net_config): removing their
 * inter-controller hops removes their traffic.
 *
 * Every hop of such a chain costs LET sampling delays (and a network
 * transfer between controllers), so removing hops is what shortens a
 * chain. For each hop, two moves are tried: the fragment after the hop
 * onto the core before it, and the fragment before the hop onto the core
 * after it. A move must respect the controller of the fragment and the
 * filling limit, and leave both cores schedulable (or no worse); it is
 * kept only if more chains are on time afterwards. A pass goes through
 * the late chains once; passes repeat while a move is kept.
 *
 * A move changes two cores only, so the chains on time are counted with
 * the incremental analysis (struct mp_communication_state): the fragments
 * of those cores and the paths of their chains are analysed again, the
 * rest is kept.
 */
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "mapping/assign.h"
#include "mapping/communication.h"
#include "mapping/let.h"
#include "mapping/placement.h"
#include "mapping/repair.h"
#include "mapping/sched.h"

#define MAX_PASSES 8

/* core index and chain slot of a fragment, or -1 */
static int locate_fragment(const struct mp_core_array *cores, int chain_id,
			   int fragment_index, int *slot)
{
	int i, j;

	for (i = 0; i < cores->count; i++)
		for (j = 0; j < cores->items[i].chains.count; j++) {
			const struct mp_chain *chain = &cores->items[i].chains.items[j];

			if (!chain->is_let && chain->id == chain_id &&
			    chain->fragment_index == fragment_index) {
				*slot = j;
				return i;
			}
		}
	return -1;
}

/*
 * chains on time after the cores of indices @first and @second changed
 * (incremental analysis), or a negative errno
 */
static int count_on_time(struct mp_communication_state *state,
			 const struct mp_core_array *cores, int first,
			 int second)
{
	int err = mp_communication_update(state, cores, first, second);

	return err ? err : state->summary.on_time_chains;
}

/* a deep copy of @source into @copy (empty) */
static int copy_paths(struct mp_path_array *copy,
		      const struct mp_path_array *source)
{
	int i;

	memset(copy, 0, sizeof(*copy));
	copy->items = calloc((size_t)source->count + 1, sizeof(*copy->items));
	if (!copy->items)
		return -ENOMEM;
	copy->capacity = source->count + 1;
	for (i = 0; i < source->count; i++) {
		const struct mp_chain_path *from = &source->items[i];
		struct mp_chain_path *to = &copy->items[i];

		*to = *from;
		to->segments = malloc(((size_t)from->segment_count + 1) *
				      sizeof(*to->segments));
		to->hops = malloc(((size_t)from->hop_count + 1) *
				  sizeof(*to->hops));
		to->segment_capacity = from->segment_count + 1;
		to->hop_capacity = from->hop_count + 1;
		copy->count++;
		if (!to->segments || !to->hops) {
			mp_path_array_free(copy);
			return -ENOMEM;
		}
		/* a chain on one core has no hop (and maybe no hop array) */
		if (from->segment_count)
			memcpy(to->segments, from->segments,
			       (size_t)from->segment_count *
			       sizeof(*to->segments));
		if (from->hop_count)
			memcpy(to->hops, from->hops,
			       (size_t)from->hop_count * sizeof(*to->hops));
	}
	return 0;
}

/* the two cores changed by a move, as they were before it */
struct move_backup {
	struct mp_core source;
	struct mp_core target;
	int source_index;
	int target_index;
};

/* put the two cores of @backup back into @cores */
static void undo_move(struct mp_core_array *cores, struct move_backup *backup)
{
	mp_core_free(&cores->items[backup->source_index]);
	mp_core_free(&cores->items[backup->target_index]);
	cores->items[backup->source_index] = backup->source;
	cores->items[backup->target_index] = backup->target;
	memset(&backup->source, 0, sizeof(backup->source));
	memset(&backup->target, 0, sizeof(backup->target));
}

/*
 * Move fragment (@chain_id, @fragment_index) to the core with id
 * @target_id, in place. When the move is possible (*@possible), @backup
 * holds the two cores as they were (undo_move() or mp_core_free() them);
 * otherwise @cores is unchanged.
 */
static int try_move(struct mp_ctx *ctx, struct mp_core_array *cores,
		    int chain_id, int fragment_index, int target_id,
		    struct move_backup *backup, bool *possible)
{
	struct mp_chain fragment = {0};
	struct mp_core *source, *target;
	int source_index, target_index, slot, load, gcd, err;
	bool source_was_ok;

	*possible = false;
	memset(backup, 0, sizeof(*backup));
	source_index = locate_fragment(cores, chain_id, fragment_index, &slot);
	target_index = mp_find_core(cores, target_id);
	if (source_index < 0 || target_index < 0 || source_index == target_index)
		return 0;
	source = &cores->items[source_index];
	target = &cores->items[target_index];

	/* the constraints of the fragment (controller, memory, replicas) */
	err = mp_chain_copy(&fragment, &source->chains.items[slot]);
	if (err)
		return err;
	if (!mp_placement_allowed(ctx, cores, &fragment, target_index, NULL))
		goto out;
	load = mp_core_load_with_chain(target, &fragment, &gcd);
	if (load > target->fill_limit)
		goto out;

	backup->source_index = source_index;
	backup->target_index = target_index;
	err = mp_core_copy(&backup->source, source);
	if (!err)
		err = mp_core_copy(&backup->target, target);
	if (err)
		goto fail;
	source_was_ok = source->schedulable;
	err = mp_core_remove_chain(source, chain_id, fragment_index);
	if (!err)
		err = mp_core_add_chain(target, &fragment, gcd);
	if (!err)
		err = mp_core_build_task_list(source, source_index);
	if (!err)
		err = mp_core_build_task_list(target, target_index);
	/*
	 * the source core keeps its priorities: with fewer tasks, every
	 * response time can only decrease
	 */
	if (!err && source->load)
		err = mp_response_times(&source->tasks, &source->schedulable);
	if (!err && source->load)
		mp_core_write_back_tasks(source);
	else if (!err)
		source->schedulable = true;	/* nothing left to schedule */
	if (!err)
		err = mp_assign_priorities_first_fit(ctx, target);
	if (err)
		goto fail;

	/* the receiving core must stay schedulable, the other no worse */
	*possible = target->schedulable &&
		    (source->schedulable || !source_was_ok);
	if (!*possible)
		undo_move(cores, backup);
out:
	mp_chain_free(&fragment);
	return err;
fail:
	if (backup->source.chains.items || backup->target.chains.items)
		undo_move(cores, backup);
	mp_chain_free(&fragment);
	return err;
}

/* the two moves around each hop of @path; true if one was kept */
static int repair_chain(struct mp_ctx *ctx, struct mp_core_array *cores,
			struct mp_communication_state *state,
			const struct mp_chain_path *path, int *on_time,
			bool *improved)
{
	int h, side, err;

	*improved = false;
	for (h = 0; h < path->hop_count; h++)
		for (side = 0; side < 2; side++) {
			const struct mp_hop *hop = &path->hops[h];
			struct move_backup backup;
			bool possible;
			int count;

			/* pull the next fragment back, or push this one on */
			err = side ? try_move(ctx, cores, path->chain_id,
					      hop->last_source_fragment,
					      hop->destination_core, &backup,
					      &possible) :
				     try_move(ctx, cores, path->chain_id,
					      hop->first_destination_fragment,
					      hop->source_core, &backup,
					      &possible);
			if (err)
				return err;
			if (!possible)
				continue;

			/*
			 * screened with first-fit priorities on the target;
			 * a promising move gets the robust ones and is
			 * counted again
			 */
			count = count_on_time(state, cores, backup.source_index,
					      backup.target_index);
			if (count > *on_time) {
				err = mp_assign_priorities(ctx,
						&cores->items[backup.target_index]);
				if (err) {
					undo_move(cores, &backup);
					return err;
				}
				count = count_on_time(state, cores,
						      backup.target_index, -1);
			}
			if (count < 0 || count <= *on_time) {
				undo_move(cores, &backup);
				if (count < 0)
					return count;
				/* the analysis follows the cores back */
				count = count_on_time(state, cores,
						      backup.source_index,
						      backup.target_index);
				if (count < 0)
					return count;
				continue;
			}
			/* keep the move: the path is now out of date */
			mp_core_free(&backup.source);
			mp_core_free(&backup.target);
			*on_time = count;
			ctx->deadline_repairs++;
			*improved = true;
			return 0;
		}
	return 0;
}

int mp_repair_deadlines(struct mp_ctx *ctx, struct mp_core_array *cores)
{
	struct mp_communication_state state;
	struct mp_path_array paths = {0};
	int pass, i, on_time, err;
	bool any_kept = true;

	err = mp_communication_init(&state, cores, &ctx->params.network);
	if (err)
		return err;
	for (pass = 0; pass < MAX_PASSES && any_kept; pass++) {
		any_kept = false;
		err = copy_paths(&paths, &state.paths);
		if (err)
			break;
		on_time = state.summary.on_time_chains;

		/*
		 * chains with all their tasks schedulable but late end to
		 * end, or through an overloaded port; after a kept move the
		 * paths are taken again and the pass goes on with the same
		 * chain (it may have other hops to remove)
		 */
		for (i = 0; i < paths.count; i++) {
			bool improved;

			if (paths.items[i].on_time || !paths.items[i].schedulable)
				continue;
			err = repair_chain(ctx, cores, &state, &paths.items[i],
					   &on_time, &improved);
			if (err)
				break;
			if (!improved)
				continue;
			any_kept = true;
			mp_path_array_free(&paths);
			err = copy_paths(&paths, &state.paths);
			if (err)
				break;
			i--;
		}
		mp_path_array_free(&paths);
		if (err)
			break;
	}
	mp_communication_state_free(&state);

	/* a core left without application tasks is empty */
	for (i = 0; i < cores->count; i++)
		cores->items[i].is_empty = !cores->items[i].load;
	return err;
}
