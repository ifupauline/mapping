/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MAPPING_NETWORK_H
#define MAPPING_NETWORK_H

/*
 * TSN network model (communication between ECUs).
 *
 * Topology. Every ECU (end station, nodes 0 to ecu_count - 1) has one
 * full-duplex Ethernet link to a switch; switches are linked to each
 * other:
 *  - star: one central switch for every ECU;
 *  - tree: one switch per ECU, the other switches linked to the switch of
 *    the root ECU (two-level backbone: the central computer of a zonal
 *    platform);
 *  - ring: one switch per ECU, the switches in a ring in ECU order;
 *  - custom: the switches and links of the configuration.
 * Routes are shortest paths (fewest links), the first link in the order
 * of the topology on ties (on a ring: clockwise). ECUs do not forward. A
 * message crosses one output port per link of its route: the egress port
 * of its ECU, then one port per switch. The link of an ECU runs at the
 * link rate, at the backbone rate for the ECUs of backbone_ecus (the
 * central computer), or at its own rate if it has one.
 *
 * Traffic. The LET messages are cut into 1 to 3 classes by the period of
 * their producer (shorter periods first), above the best-effort class
 * (assumed always busy with maximum size frames). Each port serves the
 * classes in strict priority, with either
 *  - credit-based shapers (IEEE 802.1Qav), one per class with its idle
 *    slope, or
 *  - time-aware shaping (IEEE 802.1Qbv): a cyclic gate schedule giving
 *    each LET class an exclusive window, protected by guard bands.
 * Optional frame preemption (802.1Qbu/802.3br) of the best-effort frames.
 *
 * Network Calculus (worst-case bounds):
 *  - each hop of a task-chain between two ECUs is a flow: one
 *    message per period T of its producer, release jitter J (producer WCRT
 *    + source LET period): arrival curve alpha(t) = b + r.J + r.t with
 *    b = bits of the message on the wire, r = b / T;
 *  - CBS class x on a port: rate-latency service curve with rate I_x and
 *    latency V_x / I_x + (C - I_x) l_x / (C I_x), V_x the maximum credit
 *    of the class (lower-priority blocking and higher-priority classes);
 *    the idle slopes are fixed shares of the port rate, or a budget
 *    shared at each port in proportion to the bits of each class over
 *    its shortest period;
 *  - TAS class x: rate C W_x / T_c and latency T_c - W_x, W_x the usable
 *    part of its window (the window minus one frame of the class);
 *  - delay bound at a port: latency + sum(bursts of the class) / rate, if
 *    the rates of the class fit the rate (and the design limit);
 *  - burst propagation: a flow enters the next port of its route with its
 *    burst grown by r times the bounds already crossed. On a ring the
 *    port dependencies may be cyclic: the bounds are the least fixed
 *    point of this system, reached by iteration (no bound if it
 *    diverges).
 *
 * Frame replication and elimination for reliability (IEEE 802.1CB,
 * FRER), ring or custom topology: the messages of replicated task-chains
 * are sent as two member streams. The second route has the same first and
 * last ports as the first one, and between the switch of the source ECU
 * and the switch of the destination ECU it is the shortest route that
 * uses no link and no switch of the first one (on a ring: the other way
 * around). The switch of the source replicates each frame on both
 * routes, the switch of the destination keeps the first copy of each
 * sequence number. A single failure of a link between
 * switches or of a switch other than these two leaves one member stream.
 * The bound of such a flow is the larger of the bounds over its two
 * routes, so it holds after a failure; after the elimination, the flow
 * has the burst b + r J + r D, D the largest delay of a copy from the
 * source to the elimination minus the smallest (Thomas et al., 2022).
 *
 * Rates are in bits per microsecond (1000 for 1 Gbit/s), delays in
 * microseconds.
 */

#include <stdbool.h>

#include "mapping/model.h"

/* defaults of struct mp_net_config */
#define MP_NET_LINK_MBPS		1000	/* 1000BASE-T1 zonal backbone */
#define MP_NET_IDLE_SLOPE_PERCENT	75	/* CBS idle slope (802.1Qav max) */
#define MP_NET_SWITCH_LATENCY		2	/* switch fabric + propagation, us */
#define MP_NET_MAX_CLASSES		3	/* LET classes */
#define MP_NET_TAS_CYCLE		500	/* gate cycle, us */

enum mp_net_topology {
	MP_NET_STAR,
	MP_NET_TREE,
	MP_NET_RING,
	MP_NET_CUSTOM,
};

#define MP_NET_MAX_SWITCHES	32	/* custom topology */
#define MP_NET_MAX_TRUNKS	64	/* links between two switches */

/* a full-duplex link between two switches of a custom topology */
struct mp_net_trunk {
	int from;		/* switch indices, 0 to switch_count - 1 */
	int to;
	int mbps;		/* 0: backbone_mbps */
};

enum mp_net_shaping {
	MP_NET_CBS,		/* credit-based shapers, strict priority */
	MP_NET_TAS,		/* time-aware shaping, one window per class */
	MP_NET_ATS,		/* asynchronous traffic shaping (802.1Qcr):
				 * strict priority between the classes and
				 * an interleaved regulator per flow at
				 * every switch, which restores the arrival
				 * curve of the flow at its source */
};

/* parameters of the network */
struct mp_net_config {
	enum mp_net_topology topology;	/* MP_NET_STAR */
	int ecu_count;			/* ECUs on the network (6) */
	unsigned backbone_ecus;		/* ECUs whose link runs at the
					 * backbone rate (bit 0: the PCU) */
	int ecu_mbps[MP_MAX_ECUS];	/* rate of the link of each ECU,
					 * Mbit/s (0: by the rule above) */
	int tree_root;			/* tree: the ECU whose switch is the
					 * root (0) */
	/* custom topology */
	int switch_count;
	char switch_names[MP_NET_MAX_SWITCHES][MP_NAME_SIZE];
	int ecu_switch[MP_MAX_ECUS];	/* the switch of each ECU */
	int trunk_count;
	struct mp_net_trunk trunks[MP_NET_MAX_TRUNKS];

	int link_mbps;			/* ECU links, Mbit/s */
	int backbone_mbps;		/* links between switches and the
					 * links of backbone_ecus, Mbit/s
					 * (0: link_mbps) */
	int switch_latency;		/* per switch crossed, us */
	int link_limit_percent;		/* design limit of the LET load of a
					 * port, % of its rate; a port above it
					 * is overloaded (100: none) */
	bool preemption;		/* 802.1Qbu/802.3br preemption of the
					 * best-effort frames (false) */
	enum mp_net_shaping shaping;	/* MP_NET_CBS */
	int class_count;		/* LET classes, 1 to 3 (1) */
	int class_period[MP_NET_MAX_CLASSES - 1];
					/* largest producer period of classes
					 * 0..count-2, us (5000, 20000) */
	int idle_slope_percent[MP_NET_MAX_CLASSES];
					/* CBS: idle slope of each class, % of
					 * the port rate (75 with one class;
					 * 50, 15, 10 with three) */
	bool proportional_slopes;	/* CBS, several classes: the sum of
					 * the shares divided at each port in
					 * proportion to b_k / T_k (true) */
	int tas_cycle;			/* TAS: gate cycle, us (500) */
	int tas_window_percent[MP_NET_MAX_CLASSES];
					/* TAS: window of each class, % of the
					 * cycle (70 with one class; 30, 20,
					 * 20 with three) */
	bool frer;			/* ring or custom: the replicated
					 * flows go over two routes (802.1CB)
					 * (false) */
	bool strict_let;		/* communication between cores: strict
					 * LET, an output is published at the
					 * end of the period of its producer;
					 * else implicit, copied at the first
					 * LET instant after the producer
					 * ends (false) */
};

/* Ethernet frames (802.1Q tagged), bytes */
#define MP_NET_MAX_PAYLOAD	1500
#define MP_NET_MIN_PAYLOAD	42	/* 64-byte minimum frame with VLAN tag */
#define MP_NET_OVERHEAD		42	/* header 14, tag 4, FCS 4, preamble 8,
					 * inter-frame gap 12 */
#define MP_NET_BEST_EFFORT_FRAME (MP_NET_MAX_PAYLOAD + MP_NET_OVERHEAD)
/* longest piece of a best-effort frame that cannot be preempted, bytes
 * (802.3br: 143 bytes including the overhead) */
#define MP_NET_PREEMPTION_BLOCKING 143

#define MP_NET_MAX_ROUTE	16	/* ports on a route, at most */

/* one message flow over the network, as seen by the analysis */
struct mp_flow {
	int source_controller;
	int destination_controller;
	int message_bytes;	/* payload */
	int period;		/* of the producing task, us */
	int jitter;		/* release jitter, us */
	bool replicated;	/* of a replicated task-chain: two member
				 * streams with FRER */
	bool arrival_unknown;	/* its producer misses its period: no
				 * arrival curve, so every port it crosses,
				 * and every port these feed, is unbounded */
	/* results */
	int traffic_class;	/* 0 = highest priority */
	int port_count;		/* output ports crossed */
	double source_delay;	/* bound at the egress port of the source, us */
	double switch_delay;	/* sum of the bounds at the switch ports, us */
	double total_delay;	/* all ports + switch latencies, us, -1 when
				 * a port is overloaded; with FRER the
				 * larger of the two routes */
	bool frer;		/* sent as two member streams */
	bool network_ok;	/* no overloaded port on the way */
};

/* network-wide results of one analysis */
struct mp_network_summary {
	int overloaded_flows;		/* flows crossing an overloaded port */
	int overloaded_ports;		/* ports above their limit */
	float average_delay;		/* mean bound of the other flows, us */
	float max_delay;		/* largest bound, us */
	float max_port_utilization;	/* largest LET load of a port, % */
	float max_backbone_utilization;	/* largest LET load of a link
					 * between switches, % */
	/* LET load of the ports of each ECU, % of the link rate */
	float egress_utilization[MP_MAX_ECUS];	/* ECU -> its switch */
	float ingress_utilization[MP_MAX_ECUS];	/* its switch -> ECU */
	int iterations;			/* fixed-point iterations */
	int frer_flows;			/* flows sent as two member streams */
};

/*
 * mp_net_message_bits() - bits on the wire for a message of @payload bytes
 * @max_frame_bits: set to the bits of its largest frame
 *
 * Messages larger than one frame are split; short frames are padded.
 */
int mp_net_message_bits(int payload, int *max_frame_bits);

/* the default network: one star at 1 Gbit/s, one CBS class with an idle
 * slope of 75 %, no further limit, no preemption */
void mp_net_config_default(struct mp_net_config *config);

/* set @config to @count classes with the default boundaries and shares */
void mp_net_set_classes(struct mp_net_config *config, int count);

/*
 * -EINVAL if @config is inconsistent (shares above 100 %, a custom
 * topology where two ECUs have no route, FRER on a star or a tree...),
 * else 0
 */
int mp_net_config_check(const struct mp_net_config *config);

/* the traffic class of a flow whose producer has period @period */
int mp_net_class_of(const struct mp_net_config *config, int period);

/*
 * mp_net_route() - the output ports crossed from @source to @destination
 * @links: set to the link of each port, in order (the port is at the
 *	   start of the link); at most MP_NET_MAX_ROUTE
 * Return: the number of ports (0 if @source == @destination).
 */
int mp_net_route(const struct mp_net_config *config, int source,
		 int destination, int *links);

/*
 * mp_net_secondary_route() - the second route of a replicated flow with
 * FRER: the same first and last ports as mp_net_route(), the other
 * direction around the ring between them
 * Return: the number of ports, 0 without FRER (a star or a tree,
 * @config->frer false, no such route, or @source == @destination).
 */
int mp_net_secondary_route(const struct mp_net_config *config, int source,
			   int destination, int *links);

/*
 * link @link of the topology of @config (as returned by mp_net_route):
 * its ends (nodes 0 to ecu_count - 1 are the ECUs, the switches follow) and
 * its rate in bits per us (0 if there is no such link)
 */
double mp_net_link(const struct mp_net_config *config, int link, int *from,
		   int *to);

/* number of switches of the topology of @config */
int mp_net_switch_count(const struct mp_net_config *config);

/* number of ECUs on the network of @config */
int mp_net_ecu_count(const struct mp_net_config *config);

/*
 * the CBS latency of class @class at a port of rate @rate whose classes
 * have the idle slopes @slopes (bits per us) and the largest frames
 * @frame_bits (best-effort blocking included)
 */
double mp_net_cbs_latency(const struct mp_net_config *config, double rate,
			  int class, const double *slopes,
			  const double *frame_bits);

/* number of directed links of the topology of @config */
int mp_net_link_count(const struct mp_net_config *config);

/*
 * mp_net_port_slopes() - the idle slopes the analysis of @flows gives
 * to every port: @slopes[link * MP_NET_MAX_CLASSES + class], bits per us
 * Return: 0 or -ENOMEM.
 */
int mp_net_port_slopes(const struct mp_net_config *config,
		       const struct mp_flow *flows, int count, double *slopes);

/*
 * mp_net_analysis() - Network Calculus bounds of @count flows
 *
 * Fills the result fields of every flow and @summary. Flows are taken in
 * array order (the sums are in floating point, so the order matters for
 * the last digits).
 */
void mp_net_analysis(const struct mp_net_config *config,
		     struct mp_flow *flows, int count,
		     struct mp_network_summary *summary);

/* names for the options and the outputs */
const char *mp_net_topology_name(enum mp_net_topology topology);
const char *mp_net_shaping_name(enum mp_net_shaping shaping);
int mp_net_parse_topology(const char *name, enum mp_net_topology *topology);

#endif /* MAPPING_NETWORK_H */
