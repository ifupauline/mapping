// SPDX-License-Identifier: Apache-2.0
/* Paths, network bounds and end-to-end latencies (see communication.h). */
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "mapping/array.h"
#include "mapping/communication.h"

/* a fragment as found on a core after mapping */
struct mp_placed_fragment {
	int chain_id;
	int fragment_index;
	int core_id;
	int controller;
	int last_response_time;	/* WCRT of its last task */
	int response_time;	/* sum of the WCRTs of its pattern runs */
	int sampling_delay;	/* register reads between its patterns */
	int last_period;	/* period of its last task: message period */
	int last_message_bytes;	/* message of its last task */
	int first_period;	/* period of its first task */
	int let_period;		/* period of the LET task of the core */
	int let_wcet;		/* WCET of the LET task of the core */
	int deadline;		/* of the original chain */
	int replica_group;	/* of the original chain */
	bool schedulable;	/* every task of the fragment on time */
};

struct placed_fragments {
	struct mp_placed_fragment *items;
	int count;
	int capacity;
};

static bool fragment_less(const void *a, const void *b)
{
	const struct mp_placed_fragment *first = a, *second = b;

	if (first->chain_id != second->chain_id)
		return first->chain_id < second->chain_id;
	return first->fragment_index < second->fragment_index;
}

/* the LET task of @core: its period and WCET (0 for none) */
static void core_let(const struct mp_core *core, int *let_period,
		     int *let_wcet)
{
	int j;

	*let_period = 0;
	*let_wcet = 0;
	for (j = 0; j < core->chains.count; j++) {
		const struct mp_chain *chain = &core->chains.items[j];

		if (!chain->is_let || !chain->tasks.count)
			continue;
		*let_period = chain->tasks.items[0].period;
		*let_wcet = chain->tasks.items[0].wcet;
	}
}

/* @chain, a fragment on @core (tasks in topological order) */
static void describe_fragment(const struct mp_core *core, int let_period,
			      int let_wcet, const struct mp_chain *chain,
			      struct mp_placed_fragment *fragment)
{
	const struct mp_task *last = &chain->tasks.items[chain->tasks.count - 1];
	int k;

	fragment->chain_id = chain->id;
	fragment->fragment_index = chain->fragment_index;
	fragment->core_id = core->id;
	fragment->controller = core->controller;
	fragment->last_response_time = last->response_time;
	fragment->last_period = last->period;
	fragment->last_message_bytes = last->message_bytes;
	fragment->first_period = chain->tasks.items[0].period;
	fragment->let_period = let_period;
	fragment->let_wcet = let_wcet;
	fragment->deadline = chain->deadline;
	fragment->replica_group = chain->replica_group;
	fragment->schedulable = true;
	fragment->response_time = 0;
	fragment->sampling_delay = 0;
	for (k = 0; k < chain->tasks.count; k++) {
		const struct mp_task *task = &chain->tasks.items[k];

		if (task->response_time > task->period)
			fragment->schedulable = false;
		/* a pattern run ends: its last WCRT, a read */
		if (k && task->period != chain->tasks.items[k - 1].period) {
			fragment->response_time +=
				chain->tasks.items[k - 1].response_time;
			fragment->sampling_delay += task->period;
		}
	}
	fragment->response_time += last->response_time;
}

/* every fragment of every core, by chain id then fragment index */
static int collect_fragments(const struct mp_core_array *cores,
			     struct placed_fragments *fragments)
{
	int i, j, err;

	memset(fragments, 0, sizeof(*fragments));
	for (i = 0; i < cores->count; i++) {
		const struct mp_core *core = &cores->items[i];
		int let_period, let_wcet;

		core_let(core, &let_period, &let_wcet);
		for (j = 0; j < core->chains.count; j++) {
			const struct mp_chain *chain = &core->chains.items[j];
			struct mp_placed_fragment fragment;

			if (chain->is_let || !chain->tasks.count)
				continue;
			describe_fragment(core, let_period, let_wcet, chain,
					  &fragment);
			err = mp_array_insert((void **)&fragments->items,
					      &fragments->count,
					      &fragments->capacity,
					      fragments->count, &fragment,
					      sizeof(fragment));
			if (err)
				return err;
		}
	}
	return mp_stable_sort(fragments->items, fragments->count,
			      sizeof(*fragments->items), fragment_less);
}

static int add_segment(struct mp_chain_path *path,
		       const struct mp_placed_fragment *fragment)
{
	struct mp_segment segment = {
		.core_id = fragment->core_id,
		.controller = fragment->controller,
		.first_fragment = fragment->fragment_index,
		.last_fragment = fragment->fragment_index,
		.response_time = fragment->response_time,
		.sampling_delay = fragment->sampling_delay,
		.first_period = fragment->first_period,
		.last_period = fragment->last_period,
	};

	return mp_array_insert((void **)&path->segments, &path->segment_count,
			       &path->segment_capacity, path->segment_count,
			       &segment, sizeof(segment));
}

/*
 * @next follows @previous on the core of @segment: the same pattern run
 * goes on (the WCRT of @next covers @previous), or a new pattern starts
 * after a register read
 */
static void extend_segment(struct mp_segment *segment,
			   const struct mp_placed_fragment *previous,
			   const struct mp_placed_fragment *next)
{
	if (next->first_period == previous->last_period) {
		segment->response_time += next->response_time -
					  previous->last_response_time;
	} else {
		segment->response_time += next->response_time;
		segment->sampling_delay += next->first_period;
	}
	segment->sampling_delay += next->sampling_delay;
	segment->last_fragment = next->fragment_index;
	segment->last_period = next->last_period;
}

/* the hop from @previous (last fragment of a segment) to @next */
static int add_hop(struct mp_chain_path *path,
		   const struct mp_placed_fragment *previous,
		   const struct mp_placed_fragment *next,
		   struct mp_communication_summary *summary, bool strict_let)
{
	struct mp_hop hop;

	memset(&hop, 0, sizeof(hop));
	hop.source_core = previous->core_id;
	hop.destination_core = next->core_id;
	hop.last_source_fragment = previous->fragment_index;
	hop.first_destination_fragment = next->fragment_index;

	/* written by the LET task of the source core after the producer */
	hop.flow.source_controller = previous->controller;
	hop.flow.destination_controller = next->controller;
	hop.flow.message_bytes = previous->last_message_bytes;
	hop.flow.period = previous->last_period;
	/* a producer above its period has no valid response time */
	hop.flow.arrival_unknown = previous->last_response_time >
				   previous->last_period;
	/* with FRER, the messages of replicated chains go both ways */
	hop.flow.replicated = previous->replica_group > 0;
	hop.flow.network_ok = true;
	if (strict_let) {
		/*
		 * strict LET: published at the end of the period of the
		 * producer, a LET instant of its core, and sent when the
		 * copy ends; the delay after the producer's WCRT, which the
		 * segment already counts, is what remains of its period
		 */
		hop.flow.jitter = previous->let_wcet;
		hop.write_delay = previous->last_period + previous->let_wcet -
				  previous->last_response_time;
		if (hop.write_delay < previous->let_wcet)
			hop.write_delay = previous->let_wcet;
	} else {
		/* implicit: copied at the first LET instant after the
		 * producer ends, sent when the copy ends */
		hop.flow.jitter = previous->last_response_time +
				  previous->let_period;
		hop.write_delay = previous->let_period + previous->let_wcet;
	}
	/* read by the LET task at the first release of the consumer */
	hop.read_delay = next->first_period;

	if (previous->controller == next->controller) {
		hop.type = MP_HOP_INTRA_ECU;
		summary->intra_ecu_hops++;
	} else {
		hop.type = MP_HOP_INTER_ECU;
		summary->inter_ecu_hops++;
	}
	return mp_array_insert((void **)&path->hops, &path->hop_count,
			       &path->hop_capacity, path->hop_count, &hop,
			       sizeof(hop));
}

/* the path of the chain whose fragments are @fragments[0..count) */
static int build_path(const struct mp_placed_fragment *fragments, int count,
		      struct mp_chain_path *path,
		      struct mp_communication_summary *summary, bool strict_let)
{
	int k, err;

	memset(path, 0, sizeof(*path));
	path->chain_id = fragments[0].chain_id;
	path->fragment_count = count;
	path->deadline = fragments[0].deadline;
	path->schedulable = true;
	for (k = 0; k < count; k++) {
		struct mp_segment *last_segment;

		if (fragments[k].fragment_index != k)
			return -EINVAL;	/* a fragment is not on any core */
		path->schedulable &= fragments[k].schedulable;

		/* same core as the previous fragment: same segment */
		last_segment = k ? &path->segments[path->segment_count - 1] :
			       NULL;
		if (last_segment && fragments[k].core_id == last_segment->core_id) {
			extend_segment(last_segment, &fragments[k - 1],
				       &fragments[k]);
			continue;
		}
		if (k) {
			err = add_hop(path, &fragments[k - 1], &fragments[k],
				      summary, strict_let);
			if (err)
				return err;
		}
		err = add_segment(path, &fragments[k]);
		if (err)
			return err;
	}
	return 0;
}

/* network bounds of every inter-controller hop, in path order */
static int analyse_network(const struct mp_net_config *network,
			   struct mp_path_array *paths,
			   struct mp_communication_summary *summary)
{
	struct mp_flow *flows;
	int count = 0, i, j;

	flows = malloc(((size_t)summary->inter_ecu_hops + 1) * sizeof(*flows));
	if (!flows)
		return -ENOMEM;
	for (i = 0; i < paths->count; i++)
		for (j = 0; j < paths->items[i].hop_count; j++)
			if (paths->items[i].hops[j].type == MP_HOP_INTER_ECU) {
				const struct mp_flow *flow =
					&paths->items[i].hops[j].flow;
				struct mp_flow *input = &flows[count++];

				/* the inputs only: results of an earlier
				 * analysis are dropped */
				memset(input, 0, sizeof(*input));
				input->source_controller = flow->source_controller;
				input->destination_controller =
					flow->destination_controller;
				input->message_bytes = flow->message_bytes;
				input->period = flow->period;
				input->jitter = flow->jitter;
				input->arrival_unknown = flow->arrival_unknown;
				input->replicated = flow->replicated;
				input->network_ok = true;
			}

	mp_net_analysis(network, flows, count, &summary->network);

	count = 0;
	for (i = 0; i < paths->count; i++)
		for (j = 0; j < paths->items[i].hop_count; j++)
			if (paths->items[i].hops[j].type == MP_HOP_INTER_ECU)
				paths->items[i].hops[j].flow = flows[count++];
	free(flows);
	return 0;
}

/* latencies (segment WCRTs + network bounds) and their statistics */
static void compute_latencies(struct mp_path_array *paths,
			      struct mp_communication_summary *summary)
{
	int bounded = 0, i, j;

	for (i = 0; i < paths->count; i++) {
		struct mp_chain_path *path = &paths->items[i];

		path->latency = 0;
		for (j = 0; j < path->segment_count; j++)
			path->latency += path->segments[j].response_time;
		path->e2e_delay = path->latency;
		for (j = 0; j < path->segment_count; j++)
			path->e2e_delay += path->segments[j].sampling_delay;
		for (j = 0; j < path->hop_count; j++)
			path->e2e_delay += path->hops[j].write_delay +
					  path->hops[j].read_delay;

		path->network_ok = true;
		for (j = 0; j < path->hop_count; j++) {
			const struct mp_flow *flow = &path->hops[j].flow;

			if (path->hops[j].type != MP_HOP_INTER_ECU)
				continue;
			if (!flow->network_ok) {
				path->network_ok = false;
				continue;
			}
			path->latency += (int)ceil(flow->total_delay);
			path->e2e_delay += (int)ceil(flow->total_delay);
		}
		path->reaction_time = path->e2e_delay +
				      path->segments[0].first_period;

		if (path->schedulable)
			summary->schedulable_chains++;
		else
			summary->unschedulable_chains++;

		/*
		 * timing constraint: the end-to-end deadline applies to the
		 * maximum reaction time, which equals the maximum data age
		 */
		path->meets_deadline = !path->deadline ||
				       path->reaction_time <= path->deadline;
		if (!path->meets_deadline)
			summary->deadline_misses++;
		path->on_time = path->schedulable && path->network_ok &&
				path->meets_deadline;
		if (path->on_time)
			summary->on_time_chains++;
	}
	if (paths->count) {
		summary->schedulable_chain_rate =
			((float)summary->schedulable_chains /
			 (float)paths->count) * 100;
		summary->on_time_chain_rate =
			((float)summary->on_time_chains /
			 (float)paths->count) * 100;
	}

	/*
	 * the latency of a late chain is only a lower bound, and there is no
	 * bound through an overloaded port: leave those chains out
	 */
	for (i = 0; i < paths->count; i++) {
		const struct mp_chain_path *path = &paths->items[i];

		if (!path->schedulable || !path->network_ok)
			continue;
		summary->average_latency += path->latency;
		if (path->latency > summary->max_latency)
			summary->max_latency = path->latency;
		summary->average_e2e_delay += path->e2e_delay;
		if (path->e2e_delay > summary->max_e2e_delay)
			summary->max_e2e_delay = path->e2e_delay;
		summary->average_reaction_time += path->reaction_time;
		if (path->reaction_time > summary->max_reaction_time)
			summary->max_reaction_time = path->reaction_time;
		bounded++;
	}
	if (bounded) {
		summary->average_latency /= bounded;
		summary->average_e2e_delay /= bounded;
		summary->average_reaction_time /= bounded;
	}
}

void mp_communication_state_free(struct mp_communication_state *state)
{
	mp_path_array_free(&state->paths);
	free(state->fragments);
	free(state->first_fragment);
	free(state->affected);
	memset(state, 0, sizeof(*state));
}

/* hop counts, network bounds and latencies of the paths of @state */
static int analyse_paths(struct mp_communication_state *state)
{
	struct mp_communication_summary *summary = &state->summary;
	int i, j, err;

	memset(summary, 0, sizeof(*summary));
	for (i = 0; i < state->paths.count; i++)
		for (j = 0; j < state->paths.items[i].hop_count; j++)
			if (state->paths.items[i].hops[j].type ==
			    MP_HOP_INTRA_ECU)
				summary->intra_ecu_hops++;
			else
				summary->inter_ecu_hops++;
	err = analyse_network(state->network, &state->paths, summary);
	if (!err)
		compute_latencies(&state->paths, summary);
	return err;
}

int mp_communication_init(struct mp_communication_state *state,
			  const struct mp_core_array *cores,
			  const struct mp_net_config *network)
{
	struct mp_communication_summary unused;
	struct placed_fragments fragments;
	int first, last, err;

	memset(state, 0, sizeof(*state));
	state->network = network;
	err = collect_fragments(cores, &fragments);
	state->fragments = fragments.items;
	state->fragment_count = fragments.count;
	state->first_fragment = malloc(((size_t)fragments.count + 1) *
				       sizeof(*state->first_fragment));
	state->affected = calloc((size_t)fragments.count + 1,
				 sizeof(*state->affected));
	if (!err && (!state->first_fragment || !state->affected))
		err = -ENOMEM;
	if (err)
		goto fail;

	/* one path per chain: runs of fragments with the same chain id */
	for (first = 0; first < fragments.count; first = last) {
		struct mp_chain_path path;

		for (last = first; last < fragments.count &&
		     fragments.items[last].chain_id ==
		     fragments.items[first].chain_id; last++)
			;
		state->first_fragment[state->paths.count] = first;
		err = build_path(&fragments.items[first], last - first, &path,
				 &unused, network->strict_let);
		if (!err)
			err = mp_array_insert((void **)&state->paths.items,
					      &state->paths.count,
					      &state->paths.capacity,
					      state->paths.count, &path,
					      sizeof(path));
		if (err) {
			free(path.segments);
			free(path.hops);
			goto fail;
		}
	}
	err = analyse_paths(state);
	if (!err)
		return 0;
fail:
	mp_communication_state_free(state);
	return err;
}

/* the path of chain @chain_id (paths are by increasing chain id), or -1 */
static int find_path(const struct mp_path_array *paths, int chain_id)
{
	int low = 0, high = paths->count - 1;

	while (low <= high) {
		int middle = low + (high - low) / 2;

		if (paths->items[middle].chain_id == chain_id)
			return middle;
		if (paths->items[middle].chain_id < chain_id)
			low = middle + 1;
		else
			high = middle - 1;
	}
	return -1;
}

/* describe again the fragments of @core; marks their paths */
static int refresh_core(struct mp_communication_state *state,
			const struct mp_core *core)
{
	int let_period, let_wcet, j;

	core_let(core, &let_period, &let_wcet);
	for (j = 0; j < core->chains.count; j++) {
		const struct mp_chain *chain = &core->chains.items[j];
		int path, index;

		if (chain->is_let || !chain->tasks.count)
			continue;
		path = find_path(&state->paths, chain->id);
		if (path < 0 ||
		    chain->fragment_index >= state->paths.items[path].fragment_count)
			return -EINVAL;	/* not a fragment of the mapping */
		index = state->first_fragment[path] + chain->fragment_index;
		describe_fragment(core, let_period, let_wcet, chain,
				  &state->fragments[index]);
		state->affected[path] = true;
	}
	return 0;
}

int mp_communication_update(struct mp_communication_state *state,
			    const struct mp_core_array *cores, int first,
			    int second)
{
	struct mp_communication_summary unused;
	int i, err = 0;

	if (first >= 0)
		err = refresh_core(state, &cores->items[first]);
	if (!err && second >= 0 && second != first)
		err = refresh_core(state, &cores->items[second]);
	for (i = 0; i < state->paths.count; i++) {
		struct mp_chain_path *path = &state->paths.items[i], rebuilt;

		if (!state->affected[i])
			continue;
		state->affected[i] = false;
		if (err)
			continue;
		err = build_path(&state->fragments[state->first_fragment[i]],
				 path->fragment_count, &rebuilt, &unused,
				 state->network->strict_let);
		if (err) {
			free(rebuilt.segments);
			free(rebuilt.hops);
			continue;
		}
		free(path->segments);
		free(path->hops);
		*path = rebuilt;
	}
	return err ? err : analyse_paths(state);
}

int mp_communication(const struct mp_core_array *cores,
		     const struct mp_net_config *network,
		     struct mp_path_array *paths,
		     struct mp_communication_summary *summary)
{
	struct mp_communication_state state;
	int err;

	memset(paths, 0, sizeof(*paths));
	memset(summary, 0, sizeof(*summary));
	err = mp_communication_init(&state, cores, network);
	if (err)
		return err;
	*paths = state.paths;
	*summary = state.summary;
	memset(&state.paths, 0, sizeof(state.paths));
	mp_communication_state_free(&state);
	return 0;
}

void mp_path_array_free(struct mp_path_array *paths)
{
	int i;

	for (i = 0; i < paths->count; i++) {
		free(paths->items[i].segments);
		free(paths->items[i].hops);
	}
	free(paths->items);
	memset(paths, 0, sizeof(*paths));
}
