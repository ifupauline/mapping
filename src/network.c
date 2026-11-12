// SPDX-License-Identifier: Apache-2.0
/* Network Calculus bounds of the TSN network (see network.h). */
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "mapping/model.h"
#include "mapping/network.h"

#define MAX_NODES	(MP_MAX_ECUS + MP_NET_MAX_SWITCHES)	/* ECUs, then
							 * switches */
#define MAX_LINKS	(2 * MP_MAX_ECUS + 2 * MP_NET_MAX_TRUNKS)
							/* directed links */
#define MAX_ITERATIONS	1000		/* fixed point on cyclic routes */
#define DIVERGED	1e9		/* a bound above this: no bound, us */

/* a directed link; its output port is at node @from */
struct link {
	int from;
	int to;
	double rate;		/* bits per us */
	bool backbone;		/* between two switches */
};

struct topology {
	struct link links[MAX_LINKS];
	int link_count;
	int ecu_count;		/* nodes 0 .. ecu_count - 1 */
	int switch_count;	/* the nodes after them */
};

static int ecu_count(const struct mp_net_config *config)
{
	return config->ecu_count > 0 && config->ecu_count <= MP_MAX_ECUS ?
	       config->ecu_count : MP_ZONAL_ECUS;
}

/* the switch an ECU is attached to, as a node */
static int switch_of(const struct mp_net_config *config, int ecu)
{
	int n = ecu_count(config);

	switch (config->topology) {
	case MP_NET_STAR:
		return n;
	case MP_NET_CUSTOM:
		return n + config->ecu_switch[ecu];
	default:
		return n + ecu;
	}
}

static void add_link(struct topology *topology, int from, int to,
		     double rate, bool backbone)
{
	struct link *link = &topology->links[topology->link_count++];

	link->from = from;
	link->to = to;
	link->rate = rate;
	link->backbone = backbone;
}

/* the rate of the link of @ecu to its switch, Mbit/s */
static int ecu_mbps(const struct mp_net_config *config, int ecu)
{
	int backbone = config->backbone_mbps ? config->backbone_mbps :
		       config->link_mbps;

	if (config->ecu_mbps[ecu] > 0)
		return config->ecu_mbps[ecu];
	return config->backbone_ecus & (1u << ecu) ? backbone :
	       config->link_mbps;
}

/*
 * Links in a fixed order: the ECU links first (up, then down), then the
 * backbone; on a ring the clockwise link of a switch comes before the
 * counter-clockwise one, so that routing takes it on ties; a custom
 * topology keeps the order of its links (each one way, then back).
 */
static void build_topology(const struct mp_net_config *config,
			   struct topology *topology)
{
	double backbone = config->backbone_mbps ? config->backbone_mbps :
			  config->link_mbps;
	int n = ecu_count(config), c, root;

	memset(topology, 0, sizeof(*topology));
	topology->ecu_count = n;
	topology->switch_count = mp_net_switch_count(config);
	/* the central computer is on the backbone: its link has its rate */
	for (c = 0; c < n; c++) {
		double own = ecu_mbps(config, c);

		add_link(topology, c, switch_of(config, c), own, false);
		add_link(topology, switch_of(config, c), c, own, false);
	}
	switch (config->topology) {
	case MP_NET_TREE:
		root = config->tree_root >= 0 && config->tree_root < n ?
		       config->tree_root : 0;
		for (c = 0; c < n; c++) {
			if (c == root)
				continue;
			add_link(topology, n + c, n + root, backbone, true);
			add_link(topology, n + root, n + c, backbone, true);
		}
		break;
	case MP_NET_RING:
		for (c = 0; c < n; c++) {
			int next = (c + 1) % n;
			int previous = (c + n - 1) % n;

			add_link(topology, n + c, n + next, backbone, true);
			add_link(topology, n + c, n + previous, backbone, true);
		}
		break;
	case MP_NET_CUSTOM:
		for (c = 0; c < config->trunk_count; c++) {
			const struct mp_net_trunk *t = &config->trunks[c];
			double rate = t->mbps ? t->mbps : backbone;

			add_link(topology, n + t->from, n + t->to, rate, true);
			add_link(topology, n + t->to, n + t->from, rate, true);
		}
		break;
	default:
		break;
	}
}

int mp_net_switch_count(const struct mp_net_config *config)
{
	switch (config->topology) {
	case MP_NET_STAR:
		return 1;
	case MP_NET_CUSTOM:
		return config->switch_count;
	default:
		return ecu_count(config);
	}
}

int mp_net_ecu_count(const struct mp_net_config *config)
{
	return ecu_count(config);
}

/*
 * shortest route by breadth-first search over the switches, links in
 * their order on ties; links marked in @banned (if not NULL) and nodes
 * marked in @avoid are not used; returns the number of links, 0 if
 * none (or @source == @destination)
 */
static int search(const struct topology *topology, int source,
		  int destination, const bool *banned, const bool *avoid,
		  int *links, int max)
{
	int previous_link[MAX_NODES], queue[MAX_NODES], head = 0, tail = 0;
	int count = 0, node, i;
	bool seen[MAX_NODES] = {false};

	if (source == destination)
		return 0;
	seen[source] = true;
	queue[tail++] = source;
	while (head < tail) {
		node = queue[head++];
		if (node == destination)
			break;
		/* ECUs other than the source do not forward */
		if (node < topology->ecu_count && node != source)
			continue;
		for (i = 0; i < topology->link_count; i++) {
			const struct link *link = &topology->links[i];

			if (link->from != node || seen[link->to] ||
			    (banned && banned[i]) ||
			    (avoid && avoid[link->to] &&
			     link->to != destination))
				continue;
			seen[link->to] = true;
			previous_link[link->to] = i;
			queue[tail++] = link->to;
		}
	}
	if (!seen[destination])
		return 0;
	/* walk back, then reverse */
	for (node = destination; node != source;
	     node = topology->links[previous_link[node]].from) {
		if (count == max)
			return 0;	/* longer than a route may be */
		links[count++] = previous_link[node];
	}
	for (i = 0; i < count / 2; i++) {
		int swap = links[i];

		links[i] = links[count - 1 - i];
		links[count - 1 - i] = swap;
	}
	return count;
}

static int route(const struct topology *topology, int source,
		 int destination, int *links)
{
	return search(topology, source, destination, NULL, NULL, links,
		      MP_NET_MAX_ROUTE);
}

int mp_net_route(const struct mp_net_config *config, int source,
		 int destination, int *links)
{
	struct topology topology;

	build_topology(config, &topology);
	return route(&topology, source, destination, links);
}

/*
 * FRER: the first and last ports of @primary (@count ports), and between
 * them the shortest route from the switch of the source to the switch of
 * the destination that uses no link and no switch of @primary in between
 * (on a ring: the other way around); 0 if there is none
 */
static int secondary_route(const struct mp_net_config *config,
			   const struct topology *topology, const int *primary,
			   int count, int *links)
{
	bool banned[MAX_LINKS] = {false}, avoid[MAX_NODES] = {false};
	int first, last, middle, h, n;

	if (!config->frer || (config->topology != MP_NET_RING &&
			      config->topology != MP_NET_CUSTOM) || count < 2)
		return 0;
	first = topology->links[primary[0]].to;		/* source switch */
	last = topology->links[primary[count - 1]].from; /* destination's */
	if (first == last)
		return 0;
	for (h = 0; h < count; h++) {
		const struct link *link = &topology->links[primary[h]];

		banned[primary[h]] = true;
		/* the switches between the first and the last */
		if (h > 1 && h < count - 1)
			avoid[link->from] = true;
	}
	links[0] = primary[0];
	middle = search(topology, first, last, banned, avoid, links + 1,
			MP_NET_MAX_ROUTE - 2);
	if (!middle)
		return 0;
	n = 1 + middle;
	links[n++] = primary[count - 1];
	return n;
}

int mp_net_secondary_route(const struct mp_net_config *config, int source,
			   int destination, int *links)
{
	struct topology topology;
	int primary[MP_NET_MAX_ROUTE], count;

	build_topology(config, &topology);
	count = route(&topology, source, destination, primary);
	return secondary_route(config, &topology, primary, count, links);
}

double mp_net_link(const struct mp_net_config *config, int link, int *from,
		   int *to)
{
	struct topology topology;

	build_topology(config, &topology);
	if (link < 0 || link >= topology.link_count)
		return 0;
	*from = topology.links[link].from;
	*to = topology.links[link].to;
	return topology.links[link].rate;
}

void mp_net_set_classes(struct mp_net_config *config, int count)
{
	static const int idle[3][3] = {{75}, {50, 25}, {50, 15, 10}};
	static const int windows[3][3] = {{70}, {40, 30}, {30, 20, 20}};
	int i;

	if (count < 1)
		count = 1;
	if (count > MP_NET_MAX_CLASSES)
		count = MP_NET_MAX_CLASSES;
	config->class_count = count;
	memset(config->class_period, 0, sizeof(config->class_period));
	if (count == 2) {
		config->class_period[0] = 20000;
	} else if (count == 3) {
		config->class_period[0] = 5000;
		config->class_period[1] = 20000;
	}
	for (i = 0; i < MP_NET_MAX_CLASSES; i++) {
		config->idle_slope_percent[i] = idle[count - 1][i];
		config->tas_window_percent[i] = windows[count - 1][i];
	}
}

void mp_net_config_default(struct mp_net_config *config)
{
	memset(config, 0, sizeof(*config));
	config->topology = MP_NET_STAR;
	config->ecu_count = MP_ZONAL_ECUS;
	config->backbone_ecus = 1u << MP_CONTROLLER_PCU;
	config->tree_root = MP_CONTROLLER_PCU;
	config->link_mbps = MP_NET_LINK_MBPS;
	config->backbone_mbps = 0;
	config->switch_latency = MP_NET_SWITCH_LATENCY;
	config->link_limit_percent = 100;
	config->preemption = false;
	config->shaping = MP_NET_CBS;
	config->tas_cycle = MP_NET_TAS_CYCLE;
	config->proportional_slopes = true;
	mp_net_set_classes(config, 1);
}

/* ECUs and switches in range, every ECU reachable from every other */
static int topology_check(const struct mp_net_config *config)
{
	struct topology topology;
	int links[MP_NET_MAX_ROUTE], n = config->ecu_count, c, d;

	if (n < 1 || n > MP_MAX_ECUS || config->switch_count < 0 ||
	    config->switch_count > MP_NET_MAX_SWITCHES ||
	    config->trunk_count < 0 || config->trunk_count > MP_NET_MAX_TRUNKS ||
	    (config->topology == MP_NET_TREE &&
	     (config->tree_root < 0 || config->tree_root >= n)))
		return -EINVAL;
	for (c = 0; c < n; c++)
		if (config->ecu_mbps[c] < 0)
			return -EINVAL;
	if (config->topology == MP_NET_CUSTOM) {
		if (config->switch_count < 1)
			return -EINVAL;
		for (c = 0; c < n; c++)
			if (config->ecu_switch[c] < 0 ||
			    config->ecu_switch[c] >= config->switch_count)
				return -EINVAL;
		for (c = 0; c < config->trunk_count; c++) {
			const struct mp_net_trunk *t = &config->trunks[c];

			if (t->from < 0 || t->from >= config->switch_count ||
			    t->to < 0 || t->to >= config->switch_count ||
			    t->from == t->to || t->mbps < 0)
				return -EINVAL;
		}
	}
	build_topology(config, &topology);
	for (c = 0; c < n; c++)
		for (d = 0; d < n; d++)
			if (c != d && !route(&topology, c, d, links))
				return -EINVAL;	/* no route, or too long */
	return 0;
}

int mp_net_config_check(const struct mp_net_config *config)
{
	int sum = 0, i;

	if (config->class_count < 1 || config->class_count > MP_NET_MAX_CLASSES ||
	    config->link_mbps < 1 || config->backbone_mbps < 0 ||
	    config->tas_cycle < 1 || config->link_limit_percent < 1 ||
	    (config->frer && config->topology != MP_NET_RING &&
	     config->topology != MP_NET_CUSTOM))
		return -EINVAL;
	if (topology_check(config))
		return -EINVAL;
	for (i = 0; i + 1 < config->class_count; i++)
		if (config->class_period[i] < 1 ||
		    (i && config->class_period[i] <= config->class_period[i - 1]))
			return -EINVAL;
	for (i = 0; i < config->class_count; i++) {
		int share = config->shaping == MP_NET_CBS ?
			    config->idle_slope_percent[i] :
			    config->tas_window_percent[i];

		if (config->shaping == MP_NET_ATS)
			continue;	/* no shares: strict priority */
		if (share < 1)
			return -EINVAL;
		sum += share;
	}
	return sum <= 100 ? 0 : -EINVAL;
}

int mp_net_class_of(const struct mp_net_config *config, int period)
{
	int i;

	for (i = 0; i + 1 < config->class_count; i++)
		if (period <= config->class_period[i])
			return i;
	return config->class_count - 1;
}

int mp_net_message_bits(int payload, int *max_frame_bits)
{
	int bits = 0, left = payload < 1 ? 1 : payload;

	*max_frame_bits = 0;
	while (left > 0) {
		int part = left > MP_NET_MAX_PAYLOAD ? MP_NET_MAX_PAYLOAD : left;
		int frame = ((part < MP_NET_MIN_PAYLOAD ? MP_NET_MIN_PAYLOAD : part) +
			     MP_NET_OVERHEAD) * 8;

		bits += frame;
		if (frame > *max_frame_bits)
			*max_frame_bits = frame;
		left -= part;
	}
	return bits;
}

/* bits of the best-effort frame that can block the LET classes */
static double best_effort_blocking(const struct mp_net_config *config)
{
	return (config->preemption ? MP_NET_PREEMPTION_BLOCKING :
		MP_NET_BEST_EFFORT_FRAME) * 8.0;
}

/*
 * CBS class @class at a port of rate C: rate-latency service curve with
 * rate I_x and latency V_x / I_x + (C - I_x) l_x / (C I_x), where
 *   V_x = I_x / (C (C - sum_{j<x} I_j)) (C lbar_x + sum_{j<x} (C - I_j) l_j)
 * is the largest credit of the class (Mohammadpour et al.; Zhao et al.):
 * lbar_x is the largest frame that can block it (lower LET classes and
 * best effort), l_j the largest frame of class j.
 */
double mp_net_cbs_latency(const struct mp_net_config *config, double rate,
			  int class, const double *slopes,
			  const double *frame_bits)
{
	double slope = slopes[class];
	double lower = best_effort_blocking(config), higher_slopes = 0;
	double higher_frames = 0, credit;
	int j;

	for (j = class + 1; j < config->class_count; j++)
		if (frame_bits[j] > lower)
			lower = frame_bits[j];
	for (j = 0; j < class; j++) {
		double slope_j = slopes[j];

		higher_slopes += slope_j;
		higher_frames += (rate - slope_j) * frame_bits[j];
	}
	credit = slope / (rate * (rate - higher_slopes)) *
		 (rate * lower + higher_frames);
	return credit / slope + (rate - slope) * frame_bits[class] /
	       (rate * slope);
}

/*
 * Idle slopes of the classes at a port of rate @rate. Fixed shares of
 * the rate, or (proportional_slopes, several classes) each class gets
 * its rate, and the rest of the budget of the shares is divided in
 * proportion to b_k / T_k: the bits of the messages of class k at the
 * port over its shortest period. The delay of a class is about its
 * bursts over its slope, so the extra slope goes where bursts are large
 * and periods short. Returns false if the slopes exceed the budget.
 */
static bool allocate_slopes(const struct mp_net_config *config, double rate,
			    const double *bits, const double *shortest,
			    const double *rates, double *slopes)
{
	double budget = 0, weights = 0, sum = 0, spare;
	bool proportional = config->proportional_slopes &&
			    config->class_count > 1;
	int k;

	for (k = 0; k < config->class_count; k++) {
		budget += rate * config->idle_slope_percent[k] / 100.0;
		weights += bits[k] > 0 ? bits[k] / shortest[k] : 0;
		sum += rates[k];
	}
	spare = budget - sum;
	if (proportional && (weights <= 0 || spare < 0))
		return false;
	sum = 0;
	for (k = 0; k < config->class_count; k++) {
		if (!proportional) {
			slopes[k] = rate * config->idle_slope_percent[k] / 100.0;
		} else {
			slopes[k] = rates[k];
			if (bits[k] > 0)
				slopes[k] += spare * bits[k] / shortest[k] /
					     weights;
		}
		sum += slopes[k];
	}
	return sum <= budget * (1 + 1e-12);
}

/* one class at one output port */
struct port {
	double burst_sum;	/* sum of the bursts of its flows, bits */
	double rate_sum;	/* sum of their rates, bits per us */
	double delay;		/* delay bound, us (-1 if none) */
	bool ok;		/* rates within the service, bounded bursts */
	bool fed_unbounded;	/* a flow arrives without a bound */
};

/* a flow during the analysis */
struct flow_state {
	double bits;		/* b: bits of one message on the wire */
	double rate;		/* r = b / T */
	int links[MP_NET_MAX_ROUTE];
	int link_count;
	/* FRER: the second member stream (same first and last ports) */
	int links2[MP_NET_MAX_ROUTE];
	int link_count2;	/* 0: not replicated */
	int class;
};

struct analysis {
	const struct mp_net_config *config;
	struct topology topology;
	struct port ports[MAX_LINKS][MP_NET_MAX_CLASSES];
	double frame_bits[MAX_LINKS][MP_NET_MAX_CLASSES];
	double slopes[MAX_LINKS][MP_NET_MAX_CLASSES];	/* CBS, bits/us */
	bool slopes_ok[MAX_LINKS];	/* reservations within the budget */
	double load[MAX_LINKS];		/* LET rate of each link, bits/us */
	/* the route between two controllers, once computed (count + 1) */
	int route_size[MP_MAX_ECUS][MP_MAX_ECUS];
	int routes[MP_MAX_ECUS][MP_MAX_ECUS][MP_NET_MAX_ROUTE];
	/* FRER: the second routes, the same way (count + 1) */
	int route2_size[MP_MAX_ECUS][MP_MAX_ECUS];
	int routes2[MP_MAX_ECUS][MP_MAX_ECUS][MP_NET_MAX_ROUTE];
};

/* route() between two controllers, computed once per analysis */
static int cached_route(struct analysis *a, int source, int destination,
			int *links)
{
	int *size, h;

	if (source < 0 || source >= a->topology.ecu_count || destination < 0 ||
	    destination >= a->topology.ecu_count)
		return route(&a->topology, source, destination, links);
	size = &a->route_size[source][destination];
	if (!*size)
		*size = route(&a->topology, source, destination,
			      a->routes[source][destination]) + 1;
	for (h = 0; h < *size - 1; h++)
		links[h] = a->routes[source][destination][h];
	return *size - 1;
}

/* secondary_route() between two controllers, once per analysis */
static int cached_route2(struct analysis *a, int source, int destination,
			 const int *primary, int count, int *links)
{
	int *size, h;

	if (source < 0 || source >= a->topology.ecu_count || destination < 0 ||
	    destination >= a->topology.ecu_count)
		return secondary_route(a->config, &a->topology, primary, count,
				       links);
	size = &a->route2_size[source][destination];
	if (!*size)
		*size = secondary_route(a->config, &a->topology, primary,
					count, a->routes2[source][destination]) + 1;
	for (h = 0; h < *size - 1; h++)
		links[h] = a->routes2[source][destination][h];
	return *size - 1;
}

/*
 * FRER: the delay of a member stream from its source to the elimination
 * in the switch of the destination (every port but the last, and the
 * switches), and the smallest such delay (the switch latencies); false
 * if a port on the way has no bound
 */
static bool member_delay(const struct analysis *a, const int *links,
			 int count, int class, const bool *ok_of,
			 const double *delay_of, double *worst, double *least)
{
	int h;

	*worst = 0;
	*least = (count - 1) * (double)a->config->switch_latency;
	for (h = 0; h + 1 < count; h++) {
		int index = links[h] * MP_NET_MAX_CLASSES + class;

		if (!ok_of[index])
			return false;
		*worst += delay_of[index];
	}
	*worst += *least;
	return true;
}

/* service rate and latency of class @class at the port of link @link */
static void service(const struct analysis *a, int link, int class,
		    double *rate, double *latency)
{
	const struct mp_net_config *config = a->config;
	double link_rate = a->topology.links[link].rate;

	if (config->shaping == MP_NET_ATS) {
		/*
		 * ATS: strict priority, non-preemptive. Class x is served at
		 * the rate left by the higher classes, whose flows are
		 * regulated token buckets, after the largest frame of a
		 * lower class (or best effort) and their bursts:
		 * beta = (C - r_hp) [t - (l_x + b_hp) / (C - r_hp)]^+
		 */
		double hp_rate = 0, hp_burst = 0, blocking;
		int k;

		blocking = best_effort_blocking(config);
		for (k = class + 1; k < config->class_count; k++)
			if (a->frame_bits[link][k] > blocking)
				blocking = a->frame_bits[link][k];
		for (k = 0; k < class; k++) {
			hp_rate += a->ports[link][k].rate_sum;
			hp_burst += a->ports[link][k].burst_sum;
		}
		*rate = link_rate - hp_rate;
		*latency = *rate > 0 ? (blocking + hp_burst) / *rate : 0;
		if (*rate <= 0)
			*rate = 0;
		return;
	}
	if (config->shaping == MP_NET_CBS) {
		*rate = a->slopes[link][class];
		*latency = *rate > 0 ?
			   mp_net_cbs_latency(config, link_rate, class,
					      a->slopes[link],
					      a->frame_bits[link]) : 0;
		return;
	}
	/*
	 * TAS: an exclusive window per class and cycle, guard bands keep
	 * other frames out of it, and a frame only starts if it ends
	 * within the window: W = window - l_x / C of full-rate service
	 */
	{
		double cycle = config->tas_cycle;
		double window = cycle * config->tas_window_percent[class] /
				100.0 - a->frame_bits[link][class] / link_rate;

		if (window <= 0) {
			*rate = 0;
			*latency = 0;
			return;
		}
		*rate = link_rate * window / cycle;
		*latency = cycle - window;
	}
}

/* bounds of every port from the bursts of the current iteration */
static void port_bounds(struct analysis *a)
{
	int l, k;

	for (l = 0; l < a->topology.link_count; l++) {
		double limit = a->topology.links[l].rate *
			       a->config->link_limit_percent / 100.0;

		for (k = 0; k < a->config->class_count; k++) {
			struct port *port = &a->ports[l][k];
			double rate, latency;

			service(a, l, k, &rate, &latency);
			port->ok = !port->fed_unbounded && rate > 0 &&
				   a->slopes_ok[l] &&
				   port->rate_sum < rate &&
				   a->load[l] <= limit;
			port->delay = port->ok ? latency + port->burst_sum / rate :
				      -1;
			if (port->ok && port->delay > DIVERGED) {
				port->ok = false;
				port->delay = -1;
			}
		}
	}
}

/*
 * One pass: the burst of every flow at every port of its route, from the
 * bounds of the previous pass. Returns whether a bound changed.
 */
/* one member stream: its bursts at its ports @from to @to (excluded) */
static void propagate_member(struct analysis *a, const struct flow_state *s,
			     const int *links, int from, int to, double burst,
			     bool bounded,
			     double previous[][MP_NET_MAX_CLASSES],
			     bool previous_ok[][MP_NET_MAX_CLASSES])
{
	int h;

	for (h = from; h < to; h++) {
		struct port *port = &a->ports[links[h]][s->class];

		port->burst_sum += burst;
		if (!bounded)
			port->fed_unbounded = true;
		if (!previous_ok[links[h]][s->class])
			bounded = false;
		else if (a->config->shaping != MP_NET_ATS)
			burst += s->rate * previous[links[h]][s->class];
		/* ATS: the regulators of the next switch give the flow back
		 * its source burst, without adding to the worst-case delay
		 * ("shaping for free") */
	}
}

static bool propagate(struct analysis *a, const struct mp_flow *flows,
		      const struct flow_state *states, int count)
{
	double previous[MAX_LINKS][MP_NET_MAX_CLASSES];
	bool previous_ok[MAX_LINKS][MP_NET_MAX_CLASSES];
	bool changed = false;
	int i, l, k;

	for (l = 0; l < a->topology.link_count; l++)
		for (k = 0; k < a->config->class_count; k++) {
			previous[l][k] = a->ports[l][k].delay;
			previous_ok[l][k] = a->ports[l][k].ok;
			a->ports[l][k].burst_sum = 0;
			a->ports[l][k].fed_unbounded = false;
		}
	for (i = 0; i < count; i++) {
		const struct flow_state *s = &states[i];
		double burst = s->bits + s->rate * flows[i].jitter;
		double worst, least, worst2, least2;
		struct port *last;
		bool ok;

		if (flows[i].arrival_unknown) {
			/* no arrival curve: its ports have no bound */
			propagate_member(a, s, s->links, 0, s->link_count,
					 burst, false, previous, previous_ok);
			if (s->link_count2)
				propagate_member(a, s, s->links2, 1,
						 s->link_count2, burst, false,
						 previous, previous_ok);
			continue;
		}
		if (!s->link_count2) {
			propagate_member(a, s, s->links, 0, s->link_count,
					 burst, true, previous, previous_ok);
			continue;
		}
		/*
		 * FRER: the first port once, the two member streams up to
		 * the elimination, the last port once with the burst of
		 * the eliminated flow: the source burst grown by r times
		 * the largest delay to the elimination minus the smallest
		 * (Thomas et al., 2022, corollary 1). No regulator follows
		 * the elimination.
		 */
		propagate_member(a, s, s->links, 0, s->link_count - 1, burst,
				 true, previous, previous_ok);
		propagate_member(a, s, s->links2, 1, s->link_count2 - 1,
				 a->config->shaping == MP_NET_ATS ||
				 !previous_ok[s->links[0]][s->class] ? burst :
				 burst + s->rate *
				 previous[s->links[0]][s->class],
				 previous_ok[s->links[0]][s->class], previous,
				 previous_ok);
		ok = member_delay(a, s->links, s->link_count, s->class,
				  &previous_ok[0][0], &previous[0][0], &worst,
				  &least) &&
		     member_delay(a, s->links2, s->link_count2, s->class,
				  &previous_ok[0][0], &previous[0][0], &worst2,
				  &least2);
		last = &a->ports[s->links[s->link_count - 1]][s->class];
		if (!ok) {
			last->burst_sum += burst;
			last->fed_unbounded = true;
			continue;
		}
		last->burst_sum += burst + s->rate *
				   ((worst > worst2 ? worst : worst2) -
				    (least < least2 ? least : least2));
	}
	port_bounds(a);
	for (l = 0; l < a->topology.link_count; l++)
		for (k = 0; k < a->config->class_count; k++)
			if (a->ports[l][k].ok != previous_ok[l][k] ||
			    a->ports[l][k].delay != previous[l][k])
				changed = true;
	return changed;
}

/* the analysis; @slopes (if not NULL) receives the idle slopes of every
 * link and class */
static void analyse(const struct mp_net_config *config, struct mp_flow *flows,
		    int count, struct mp_network_summary *summary,
		    double *slopes)
{
	struct analysis *a;
	struct flow_state *states;
	int bounded = 0, i, l, k, h, frame;
	bool changed = true;

	memset(summary, 0, sizeof(*summary));
	a = calloc(1, sizeof(*a));	/* large: not on the stack */
	states = count ? calloc((size_t)count, sizeof(*states)) : NULL;
	if (!a || (count && !states)) {
		/* no memory: no bound for anybody */
		for (i = 0; i < count; i++) {
			flows[i].network_ok = false;
			flows[i].total_delay = -1;
		}
		summary->overloaded_flows = count;
		free(a);
		free(states);
		return;
	}
	a->config = config;
	build_topology(config, &a->topology);

	/* routes, classes, rates and the largest frames of every port */
	for (i = 0; i < count; i++) {
		struct flow_state *s = &states[i];

		s->bits = mp_net_message_bits(flows[i].message_bytes, &frame);
		s->rate = s->bits / flows[i].period;
		s->class = mp_net_class_of(config, flows[i].period);
		s->link_count = cached_route(a, flows[i].source_controller,
					     flows[i].destination_controller,
					     s->links);
		s->link_count2 = flows[i].replicated ?
			cached_route2(a, flows[i].source_controller,
				      flows[i].destination_controller,
				      s->links, s->link_count, s->links2) : 0;
		for (h = 0; h < s->link_count; h++) {
			struct port *port = &a->ports[s->links[h]][s->class];

			port->rate_sum += s->rate;
			a->load[s->links[h]] += s->rate;
			if (frame > a->frame_bits[s->links[h]][s->class])
				a->frame_bits[s->links[h]][s->class] = frame;
		}
		/* the second member stream, between the first and last port */
		for (h = 1; h + 1 < s->link_count2; h++) {
			struct port *port = &a->ports[s->links2[h]][s->class];

			port->rate_sum += s->rate;
			a->load[s->links2[h]] += s->rate;
			if (frame > a->frame_bits[s->links2[h]][s->class])
				a->frame_bits[s->links2[h]][s->class] = frame;
		}
	}

	/* idle slopes of every port */
	{
		double bits[MAX_LINKS][MP_NET_MAX_CLASSES] = {{0}};
		double shortest[MAX_LINKS][MP_NET_MAX_CLASSES] = {{0}};

		for (i = 0; i < count; i++)
			for (h = 0; h < states[i].link_count +
			     (states[i].link_count2 ? states[i].link_count2 - 2 : 0);
			     h++) {
				/* the ports of the second member stream follow */
				int link = h < states[i].link_count ?
					   states[i].links[h] :
					   states[i].links2[h - states[i].link_count + 1];
				int class = states[i].class;

				bits[link][class] += states[i].bits;
				if (!shortest[link][class] ||
				    flows[i].period < shortest[link][class])
					shortest[link][class] = flows[i].period;
			}
		for (l = 0; l < a->topology.link_count; l++) {
			double rates[MP_NET_MAX_CLASSES];

			for (k = 0; k < config->class_count; k++)
				rates[k] = a->ports[l][k].rate_sum;
			a->slopes_ok[l] = config->shaping == MP_NET_ATS ||
				allocate_slopes(config,
						a->topology.links[l].rate,
						bits[l], shortest[l], rates,
						a->slopes[l]);
		}
	}

	/*
	 * Least fixed point of the bounds: start from no propagation and
	 * iterate (feed-forward routes converge in as many passes as the
	 * longest route; on a ring the bounds grow until they settle or
	 * exceed DIVERGED)
	 */
	for (l = 0; l < a->topology.link_count; l++)
		for (k = 0; k < config->class_count; k++) {
			a->ports[l][k].delay = 0;
			a->ports[l][k].ok = true;
		}
	for (summary->iterations = 0;
	     changed && summary->iterations < MAX_ITERATIONS;
	     summary->iterations++)
		changed = propagate(a, flows, states, count);
	/* not settled: the bounds found so far are not bounds */
	if (changed)
		for (l = 0; l < a->topology.link_count; l++)
			for (k = 0; k < config->class_count; k++) {
				a->ports[l][k].ok = false;
				a->ports[l][k].delay = -1;
			}

	/* bound of each flow: its ports plus the switch latencies */
	for (i = 0; i < count; i++) {
		struct mp_flow *flow = &flows[i];
		const struct flow_state *s = &states[i];
		double total = 0, switches = 0;
		bool ok = s->link_count > 0;

		flow->traffic_class = s->class;
		flow->port_count = s->link_count;
		for (h = 0; h < s->link_count; h++) {
			const struct port *port = &a->ports[s->links[h]][s->class];

			ok &= port->ok;
			if (h) {
				total += config->switch_latency;
				switches += port->delay;
			}
			total += port->delay;
		}
		/* FRER: the larger of the two routes (after a failure the
		 * other one may be all there is) */
		flow->frer = s->link_count2 > 0;
		if (flow->frer) {
			double total2 = 0, switches2 = 0;

			summary->frer_flows++;
			for (h = 0; h < s->link_count2; h++) {
				const struct port *port =
					&a->ports[s->links2[h]][s->class];

				ok &= port->ok;
				if (h) {
					total2 += config->switch_latency;
					switches2 += port->delay;
				}
				total2 += port->delay;
			}
			if (total2 > total) {
				total = total2;
				switches = switches2;
			}
		}
		flow->source_delay = s->link_count ?
			a->ports[s->links[0]][s->class].delay : 0;
		flow->switch_delay = switches;
		if (!ok) {
			flow->network_ok = false;
			flow->total_delay = -1;
			summary->overloaded_flows++;
			continue;
		}
		flow->network_ok = true;
		flow->total_delay = total;
		summary->average_delay += flow->total_delay;
		if (flow->total_delay > summary->max_delay)
			summary->max_delay = flow->total_delay;
		bounded++;
	}
	if (bounded)
		summary->average_delay /= bounded;

	/* loads of the ports */
	for (l = 0; l < a->topology.link_count; l++) {
		const struct link *link = &a->topology.links[l];
		float load = (float)(a->load[l] / link->rate * 100);

		for (k = 0; k < config->class_count; k++)
			summary->overloaded_ports += !a->ports[l][k].ok;
		if (link->from < a->topology.ecu_count)
			summary->egress_utilization[link->from] = load;
		else if (link->to < a->topology.ecu_count)
			summary->ingress_utilization[link->to] = load;
		if (load > summary->max_port_utilization)
			summary->max_port_utilization = load;
		if (link->backbone && load > summary->max_backbone_utilization)
			summary->max_backbone_utilization = load;
		if (slopes)
			for (k = 0; k < MP_NET_MAX_CLASSES; k++)
				slopes[l * MP_NET_MAX_CLASSES + k] =
					a->slopes[l][k];
	}
	free(states);
	free(a);
}

void mp_net_analysis(const struct mp_net_config *config,
		     struct mp_flow *flows, int count,
		     struct mp_network_summary *summary)
{
	analyse(config, flows, count, summary, NULL);
}

int mp_net_link_count(const struct mp_net_config *config)
{
	struct topology topology;

	build_topology(config, &topology);
	return topology.link_count;
}

int mp_net_port_slopes(const struct mp_net_config *config,
		       const struct mp_flow *flows, int count, double *slopes)
{
	struct mp_network_summary summary;
	struct mp_flow *copy = malloc(((size_t)count + 1) * sizeof(*copy));

	if (!copy)
		return -ENOMEM;
	memcpy(copy, flows, (size_t)count * sizeof(*copy));
	analyse(config, copy, count, &summary, slopes);
	free(copy);
	return 0;
}

const char *mp_net_topology_name(enum mp_net_topology topology)
{
	switch (topology) {
	case MP_NET_TREE:
		return "tree";
	case MP_NET_RING:
		return "ring";
	case MP_NET_CUSTOM:
		return "custom";
	default:
		return "star";
	}
}

const char *mp_net_shaping_name(enum mp_net_shaping shaping)
{
	return shaping == MP_NET_TAS ? "TAS" :
	       shaping == MP_NET_ATS ? "ATS" : "CBS";
}

int mp_net_parse_topology(const char *name, enum mp_net_topology *topology)
{
	static const enum mp_net_topology all[] = {MP_NET_STAR, MP_NET_TREE,
						   MP_NET_RING};
	int i;

	for (i = 0; i < 3; i++)
		if (!strcmp(name, mp_net_topology_name(all[i]))) {
			*topology = all[i];
			return 0;
		}
	return -EINVAL;
}
