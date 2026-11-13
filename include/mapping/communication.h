/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MAPPING_COMMUNICATION_H
#define MAPPING_COMMUNICATION_H

#include "mapping/context.h"
#include "mapping/model.h"
#include "mapping/network.h"

/*
 * Communication of the task-chains after mapping.
 *
 * The fragments of a task-chain are followed core by core. Consecutive
 * fragments on the same core form a segment; between two segments a
 * message is exchanged through the LET tasks: inside a controller (shared
 * memory) or between two controllers (over the TSN network).
 *
 * Inside a segment, the tasks of one activation pattern are released
 * together and run in chain order (their priorities decrease along the
 * chain), so the WCRT of the last one covers them. Between two patterns
 * the data goes through a register: the consumer reads it at its next
 * release, at most one consumer period later (a sampling delay).
 *
 * End-to-end latency of a chain = sum over its segments of the WCRT of
 * the last task of every pattern run + the network bound of every
 * inter-controller hop.
 *
 * End-to-end delay (from the release of the job of the first task that
 * samples the sensor to the end of the first job of the last task that
 * uses that sample, "first-to-first") adds the sampling delays: the
 * register reads inside the segments, and the LET communication at every
 * hop:
 *  - write: the output waits for the next LET instant of the producer
 *    core (at most its LET period) and for the copy (the LET WCET);
 *  - read: the consumer LET task copies the data at the release of the
 *    first task of the next segment, which comes at most one period of
 *    that task later (the copy itself is in that task's WCRT).
 * Reaction time bound = end-to-end delay + one period of the first task (an
 * external event waits at most that long to be sampled). The maximum
 * reaction time equals the maximum data age ("last-to-last"; Guenzel et
 * al., ECRTS 2023), so it also bounds the data age.
 *
 * A chain is on time when it is schedulable (every task meets its
 * deadline, the period), no hop crosses an overloaded port and its
 * maximum reaction time meets its end-to-end deadline.
 */

#define MP_HOP_INTRA_ECU	0	/* two cores of one controller */
#define MP_HOP_INTER_ECU	1	/* two controllers, over the network */

/* a maximal run of consecutive fragments of a chain on one core */
struct mp_segment {
	int core_id;
	int controller;
	int first_fragment;	/* fragment indices, inclusive */
	int last_fragment;
	int response_time;	/* sum of the WCRTs of its pattern runs, us */
	int sampling_delay;	/* register reads between its patterns, us */
	int first_period;	/* period of its first task, us */
	int last_period;	/* period of its last task, us */
};

/* the message between two consecutive segments */
struct mp_hop {
	int source_core;	/* core ids */
	int destination_core;
	int last_source_fragment;	/* fragment before the hop */
	int first_destination_fragment;	/* fragment after it */
	int type;		/* MP_HOP_INTRA_ECU or MP_HOP_INTER_ECU */
	/*
	 * message: produced by the last task of the source segment, period
	 * of that task, jitter = its WCRT + the source LET period; network
	 * bounds for an inter-controller hop (zero otherwise)
	 */
	struct mp_flow flow;
	int write_delay;	/* producer LET period + LET WCET, us */
	int read_delay;		/* period of the first consumer task, us */
};

/* the path and latency of one original task-chain */
struct mp_chain_path {
	int chain_id;
	int fragment_count;
	bool schedulable;	/* every task of the chain meets its period */
	bool network_ok;	/* no hop through an overloaded port */
	int deadline;		/* end-to-end deadline, us (0: none) */
	bool meets_deadline;	/* no deadline, or reaction time <= deadline */
	bool on_time;		/* schedulable, network ok, deadline met */
	int latency;		/* end-to-end latency, us */
	int e2e_delay;		/* LET-aware end-to-end delay bound, us */
	int reaction_time;	/* LET-aware reaction time bound, us */
	struct mp_segment *segments;
	int segment_count;
	int segment_capacity;
	struct mp_hop *hops;
	int hop_count;
	int hop_capacity;
};

struct mp_path_array {
	struct mp_chain_path *items;
	int count;
	int capacity;
};

/* results of the communication phase of a design point */
struct mp_communication_summary {
	int intra_ecu_hops;
	int inter_ecu_hops;
	int schedulable_chains;
	int unschedulable_chains;
	float schedulable_chain_rate;	/* %, schedulable chains */
	int deadline_misses;		/* chains whose reaction time exceeds
					 * their deadline */
	int on_time_chains;		/* see struct mp_chain_path */
	float on_time_chain_rate;	/* % */
	float average_latency;		/* us, chains schedulable and bounded */
	float max_latency;		/* us, same chains */
	float average_e2e_delay;		/* us, same chains */
	float max_e2e_delay;
	float average_reaction_time;
	float max_reaction_time;
	struct mp_network_summary network;
};

/*
 * mp_communication() - paths, network bounds and latencies of every chain
 * @paths: filled with one path per original chain, by increasing chain id
 *
 * Return: 0, -EINVAL if a fragment is missing from the cores, -ENOMEM.
 */
int mp_communication(const struct mp_core_array *cores,
		     const struct mp_net_config *network,
		     struct mp_path_array *paths,
		     struct mp_communication_summary *summary);

void mp_path_array_free(struct mp_path_array *paths);

/*
 * Incremental analysis: the results of mp_communication() kept up to date
 * while fragments move. A move changes the fragments of two cores only
 * (their response times, and the core of the moved fragment): only those
 * fragments are described again and only the paths of their chains are
 * rebuilt. The network is analysed again as a whole (a flow interferes
 * with every flow it shares a port with, and bursts propagate) and the
 * latencies are summed again, both linear in the number of hops. The
 * results are exactly those of mp_communication() on the same cores.
 */
struct mp_placed_fragment;

struct mp_communication_state {
	struct mp_path_array paths;		/* as mp_communication() */
	struct mp_communication_summary summary;
	/* private */
	const struct mp_net_config *network;
	struct mp_placed_fragment *fragments;	/* by chain, then index */
	int fragment_count;
	int *first_fragment;	/* per path: index of its first fragment */
	bool *affected;		/* per path, scratch */
};

/* full analysis of @cores; @network must outlive @state */
int mp_communication_init(struct mp_communication_state *state,
			  const struct mp_core_array *cores,
			  const struct mp_net_config *network);

/*
 * after a change of the cores of indices @first and @second (-1: none) in
 * @cores, the results are those of the new mapping
 */
int mp_communication_update(struct mp_communication_state *state,
			    const struct mp_core_array *cores, int first,
			    int second);

void mp_communication_state_free(struct mp_communication_state *state);

#endif /* MAPPING_COMMUNICATION_H */
