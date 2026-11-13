// SPDX-License-Identifier: Apache-2.0
/* Unit tests of the TSN network bounds and of the chain paths. */
#include <errno.h>
#include <math.h>
#include <string.h>

#include "mapping/assign.h"
#include "mapping/communication.h"
#include "mapping/let.h"
#include "unit.h"

/* the default network, set by main() */
static struct mp_net_config network;

/* CBS latency of the single class of a 1 Gbit/s port, frame @bits */
static double cbs_latency(const struct mp_net_config *config, double bits)
{
	double frames[MP_NET_MAX_CLASSES] = {bits};
	double slopes[MP_NET_MAX_CLASSES] = {750};

	return mp_net_cbs_latency(config, 1000, 0, slopes, frames);
}

static void test_message_bits(void)
{
	int largest;

	/* short messages are padded to the 64-byte minimum frame */
	CHECK_EQ(mp_net_message_bits(10, &largest), (42 + 42) * 8);
	CHECK_EQ(mp_net_message_bits(0, &largest), (42 + 42) * 8);
	CHECK_EQ(mp_net_message_bits(100, &largest), 142 * 8);
	CHECK_EQ(largest, 142 * 8);
	/* 3000 bytes: two full frames */
	CHECK_EQ(mp_net_message_bits(3000, &largest), 2 * 1542 * 8);
	CHECK_EQ(largest, 1542 * 8);
}

/*
 * One flow of 100 bytes every 1000 us without jitter, PCU -> ZCU1:
 * b = 1136 bits, r = 1.136 bit/us, I = 750 bit/us.
 *   CBS latency L = 1542 * 8 / 1000 + 250 * 1136 / 750000 = 12.714667
 *   source port   = L + 1136 / 750                       = 14.229333
 *   switch port   = L + (1136 + 1.136 * 14.229333) / 750 = 14.250886
 *   total         = 14.229333 + 2 + 14.250886            = 30.480219
 */
static void test_single_flow_bound(void)
{
	struct mp_flow flow = {
		.source_controller = MP_CONTROLLER_PCU,
		.destination_controller = MP_CONTROLLER_ZCU1,
		.message_bytes = 100, .period = 1000, .jitter = 0,
	};
	struct mp_network_summary summary;

	CHECK_NEAR(cbs_latency(&network, 1136), 12.714667, 1e-5);
	mp_net_analysis(&network, &flow, 1, &summary);
	CHECK(flow.network_ok);
	CHECK_NEAR(flow.source_delay, 14.229333, 1e-4);
	CHECK_NEAR(flow.switch_delay, 14.250886, 1e-4);
	CHECK_NEAR(flow.total_delay, 30.480219, 1e-4);
	CHECK_EQ(summary.overloaded_flows, 0);
	CHECK_NEAR(summary.max_delay, flow.total_delay, 1e-6);
	CHECK_NEAR(summary.max_port_utilization, 0.1136, 1e-5);
}

/*
 * Frame preemption (802.1Qbu): a LET frame waits for at most 143 bytes of
 * a best-effort frame instead of a whole 1542-byte one.
 *   L = 143 * 8 / 1000 + 250 * 1136 / 750000 = 1.522667
 */
static void test_frame_preemption(void)
{
	struct mp_net_config preemptive = network;
	struct mp_flow flow = {
		.source_controller = MP_CONTROLLER_PCU,
		.destination_controller = MP_CONTROLLER_ZCU1,
		.message_bytes = 100, .period = 1000, .jitter = 0,
	};
	struct mp_network_summary summary;

	preemptive.preemption = true;
	CHECK_NEAR(cbs_latency(&preemptive, 1136), 1.522667, 1e-5);
	mp_net_analysis(&preemptive, &flow, 1, &summary);
	CHECK(flow.network_ok);
	CHECK_NEAR(flow.source_delay, 3.037333, 1e-4);
	CHECK_NEAR(flow.total_delay, 8.079267, 1e-4);
}

/* jitter grows the burst: a later flow sharing the port waits longer */
static void test_jitter_and_sharing(void)
{
	struct mp_flow flows[2] = {
		{ .source_controller = 0, .destination_controller = 1,
		  .message_bytes = 100, .period = 1000, .jitter = 0 },
		{ .source_controller = 0, .destination_controller = 2,
		  .message_bytes = 100, .period = 1000, .jitter = 500 },
	};
	struct mp_network_summary summary;
	double burst_sum = 1136 + (1136 + 1.136 * 500);

	mp_net_analysis(&network, flows, 2, &summary);
	/* both flows share the PCU egress port: same bound there */
	CHECK_NEAR(flows[0].source_delay, flows[1].source_delay, 1e-6);
	CHECK_NEAR(flows[0].source_delay,
		   cbs_latency(&network, 1136) + burst_sum / 750, 1e-4);
	/* separate switch ports: the jittery flow has the larger bound */
	CHECK(flows[1].switch_delay > flows[0].switch_delay);
}

/*
 * a flow whose producer misses its period has no arrival curve: the ports
 * it crosses have no bound, for every flow there, and neither have the
 * ports that these feed; a flow on other ports keeps its bound
 */
static void test_unknown_arrival(void)
{
	struct mp_flow flows[3] = {
		{ .source_controller = 0, .destination_controller = 1,
		  .message_bytes = 100, .period = 1000, .jitter = 0,
		  .arrival_unknown = true },
		{ .source_controller = 0, .destination_controller = 2,
		  .message_bytes = 100, .period = 1000, .jitter = 0 },
		{ .source_controller = 3, .destination_controller = 4,
		  .message_bytes = 100, .period = 1000, .jitter = 0 },
	};
	struct mp_network_summary summary;

	mp_net_analysis(&network, flows, 3, &summary);
	CHECK(!flows[0].network_ok);
	CHECK(!flows[1].network_ok);	/* shares the PCU egress port */
	CHECK(flows[2].network_ok);
	CHECK_EQ(summary.overloaded_flows, 2);
}

/* above the idle slope (750 bit/us) a port has no bound */
static void test_overloaded_port(void)
{
	struct mp_flow flow = {
		.source_controller = 3, .destination_controller = 0,
		.message_bytes = 1500, .period = 10,
	};
	struct mp_network_summary summary;

	mp_net_analysis(&network, &flow, 1, &summary);
	CHECK(!flow.network_ok);
	CHECK_NEAR(flow.total_delay, -1, 1e-9);
	CHECK_EQ(summary.overloaded_flows, 1);
}

/* a design limit on the port load, and a slower link */
static void test_link_limit_and_rate(void)
{
	struct mp_flow flow = {
		.source_controller = 0, .destination_controller = 1,
		.message_bytes = 1500, .period = 100,	/* 123.36 bit/us */
	};
	struct mp_net_config limited = network, slow = network;
	struct mp_network_summary summary;
	float fast_delay;

	mp_net_analysis(&network, &flow, 1, &summary);
	CHECK(flow.network_ok);
	fast_delay = flow.total_delay;

	limited.link_limit_percent = 10;	/* 100 bit/us at most */
	mp_net_analysis(&limited, &flow, 1, &summary);
	CHECK(!flow.network_ok);

	slow.link_mbps = 100;			/* 75 bit/us idle slope */
	mp_net_analysis(&slow, &flow, 1, &summary);
	CHECK(!flow.network_ok);
	flow.period = 1000;			/* 12.3 bit/us */
	mp_net_analysis(&slow, &flow, 1, &summary);
	CHECK(flow.network_ok);
	CHECK(flow.total_delay > fast_delay);
}

/* put fragment @index of chain 0 (one task of period @period) on @core */
static void place_fragment_with_period(struct mp_core *core, int index,
				       int response_time, int period)
{
	struct mp_chain fragment;
	struct mp_task task;
	int gcd;

	memset(&fragment, 0, sizeof(fragment));
	memset(&task, 0, sizeof(task));
	task.wcet = 100;
	task.reference_wcet = 100;
	task.period = period;
	task.utilization = mp_utilization(100, period);
	task.id_in_chain = index;
	task.message_bytes = 100;
	task.response_time = response_time;
	mp_task_array_push(&fragment.tasks, &task);
	fragment.fragment_index = index;
	fragment.utilization = task.utilization;
	mp_core_load_with_chain(core, &fragment, &gcd);
	mp_core_add_chain(core, &fragment, gcd);
	mp_chain_free(&fragment);
}

static void place_fragment(struct mp_core *core, int index, int response_time)
{
	place_fragment_with_period(core, index, response_time, 1000);
}

/*
 * Chain 0 in four fragments: 0 and 1 on PCU core 0, 2 on PCU core 1,
 * 3 on a ZCU1 core: two segments joined in the PCU, one network hop.
 */
static void test_chain_path_and_latency(void)
{
	struct mp_core_array cores = {0};
	struct mp_path_array paths;
	struct mp_communication_summary summary;
	const struct mp_chain_path *path;
	struct mp_ctx ctx;

	mp_ctx_init(&ctx, 1);
	ctx.next_chain_id = 1;
	mp_add_core(&ctx, &cores, MP_CONTROLLER_PCU);
	mp_add_core(&ctx, &cores, MP_CONTROLLER_PCU);
	mp_add_core(&ctx, &cores, MP_CONTROLLER_ZCU1);
	place_fragment(&cores.items[0], 0, 300);
	place_fragment(&cores.items[0], 1, 500);
	place_fragment(&cores.items[1], 2, 400);
	place_fragment(&cores.items[2], 3, 200);

	CHECK_EQ(mp_communication(&cores, &network, &paths, &summary), 0);
	CHECK_EQ(paths.count, 1);
	path = &paths.items[0];
	CHECK_EQ(path->fragment_count, 4);
	CHECK_EQ(path->segment_count, 3);
	CHECK_EQ(path->segments[0].last_fragment, 1);
	CHECK_EQ(path->segments[0].response_time, 500);
	CHECK_EQ(path->hop_count, 2);
	CHECK_EQ(path->hops[0].type, MP_HOP_INTRA_ECU);
	CHECK_EQ(path->hops[1].type, MP_HOP_INTER_ECU);
	/* jitter: producer WCRT 400 + LET period of core 1 (1000) */
	CHECK_EQ(path->hops[1].flow.jitter, 1400);
	CHECK(path->schedulable && path->network_ok);
	CHECK_EQ(path->latency, 500 + 400 + 200 +
		 (int)ceil(path->hops[1].flow.total_delay));
	/*
	 * end-to-end delay: + LET period 1000 + LET WCET 50 on the producer side and
	 * the 1000 us period of the consumer, at both hops
	 */
	CHECK_EQ(path->hops[0].write_delay, 1050);
	CHECK_EQ(path->hops[0].read_delay, 1000);
	CHECK_EQ(path->segments[0].sampling_delay, 0);
	CHECK_EQ(path->e2e_delay, path->latency + 2 * (1050 + 1000));
	CHECK_EQ(path->reaction_time, path->e2e_delay + 1000);
	CHECK_EQ(summary.intra_ecu_hops, 1);
	CHECK_EQ(summary.inter_ecu_hops, 1);
	mp_path_array_free(&paths);

	/* a missing fragment is an error */
	mp_core_remove_chain(&cores.items[1], 0, 2);
	CHECK_EQ(mp_communication(&cores, &network, &paths, &summary), -EINVAL);
	mp_core_array_free(&cores);
}

/*
 * Two patterns on one core: fragment 0 (T = 1000, R = 300), then fragment 1
 * (T = 5000, R = 700). No hop, but a register between the patterns: the
 * latency adds both WCRTs, the end-to-end delay one consumer period more.
 */
static void test_patterns_on_one_core(void)
{
	struct mp_core_array cores = {0};
	struct mp_path_array paths;
	struct mp_communication_summary summary;
	const struct mp_chain_path *path;
	struct mp_ctx ctx;

	mp_ctx_init(&ctx, 1);
	ctx.next_chain_id = 1;
	mp_add_core(&ctx, &cores, MP_CONTROLLER_PCU);
	place_fragment_with_period(&cores.items[0], 0, 300, 1000);
	place_fragment_with_period(&cores.items[0], 1, 700, 5000);

	CHECK_EQ(mp_communication(&cores, &network, &paths, &summary), 0);
	path = &paths.items[0];
	CHECK_EQ(path->segment_count, 1);
	CHECK_EQ(path->hop_count, 0);
	CHECK_EQ(path->segments[0].response_time, 300 + 700);
	CHECK_EQ(path->segments[0].sampling_delay, 5000);
	CHECK_EQ(path->latency, 1000);
	CHECK_EQ(path->e2e_delay, 6000);
	CHECK_EQ(path->reaction_time, 7000);
	mp_path_array_free(&paths);
	mp_core_array_free(&cores);
}

/* routes of the three topologies (ports crossed) */
static void test_routes(void)
{
	struct mp_net_config config = network;
	int links[MP_NET_MAX_ROUTE];

	CHECK_EQ(mp_net_route(&config, 1, 4, links), 2);	/* star */
	CHECK_EQ(mp_net_route(&config, 2, 2, links), 0);
	config.topology = MP_NET_TREE;
	CHECK_EQ(mp_net_switch_count(&config), 6);
	CHECK_EQ(mp_net_route(&config, 0, 3, links), 3);	/* PCU, P, Z3 */
	CHECK_EQ(mp_net_route(&config, 1, 4, links), 4);	/* via P */
	config.topology = MP_NET_RING;
	CHECK_EQ(mp_net_route(&config, 0, 1, links), 3);
	CHECK_EQ(mp_net_route(&config, 1, 5, links), 4);	/* via the PCU */
	CHECK_EQ(mp_net_route(&config, 1, 4, links), 5);	/* tie: clockwise */
	CHECK_EQ(mp_net_route(&config, 4, 1, links), 5);
}

/*
 * Two CBS classes (idle slopes of 40 and 35 % given here), 1136-bit
 * frames in both,
 * best effort 12336 bits:
 *   class 0: V = 400 x 12336 / 1000 = 4934.4,
 *            L = 4934.4 / 400 + 600 x 1136 / 400000 = 14.04
 *   class 1: V = 350 / (1000 x 600) x (1000 x 12336 + 600 x 1136)
 *              = 7593.6,
 *            L = 7593.6 / 350 + 650 x 1136 / 350000 = 23.805714
 */
static void test_two_cbs_classes(void)
{
	struct mp_net_config config = network;
	double frames[MP_NET_MAX_CLASSES] = {1136, 1136, 0};
	double slopes[MP_NET_MAX_CLASSES] = {400, 350, 0};

	mp_net_set_classes(&config, 2);
	CHECK_EQ(mp_net_config_check(&config), 0);
	CHECK_EQ(mp_net_class_of(&config, 20000), 0);
	CHECK_EQ(mp_net_class_of(&config, 50000), 1);
	CHECK_NEAR(mp_net_cbs_latency(&config, 1000, 0, slopes, frames), 14.04, 1e-6);
	CHECK_NEAR(mp_net_cbs_latency(&config, 1000, 1, slopes, frames),
		   23.805714,
		   1e-5);
	/* one class takes the bound of version 2.0 */
	CHECK_NEAR(cbs_latency(&network, 1136), 12.714667, 1e-5);
	config.idle_slope_percent[1] = 60;	/* 110 % in all */
	CHECK_EQ(mp_net_config_check(&config), -EINVAL);
}

/*
 * Proportional idle slopes, two classes at the PCU egress port: a
 * 1000-bit message every 1 ms (class 0) and every 50 ms (class 1).
 * Budget 750 bit/us; each class gets its rate (1 and 0.02), the spare
 * 748.98 goes in proportion to b / T: 1 and 0.02.
 */
static void test_proportional_slopes(void)
{
	struct mp_net_config config = network;
	struct mp_flow flows[2];
	double slopes[64 * MP_NET_MAX_CLASSES];
	int i;

	mp_net_set_classes(&config, 2);
	config.idle_slope_percent[0] = 40;
	config.idle_slope_percent[1] = 35;
	for (i = 0; i < 2; i++) {
		memset(&flows[i], 0, sizeof(flows[i]));
		flows[i].source_controller = MP_CONTROLLER_PCU;
		flows[i].destination_controller = MP_CONTROLLER_ZCU1;
		flows[i].message_bytes = 83;		/* 1000 bits */
		flows[i].period = i ? 50000 : 1000;
	}
	CHECK_EQ(mp_net_port_slopes(&config, flows, 2, slopes), 0);
	/* link 0: PCU -> its switch */
	CHECK_NEAR(slopes[0], 1 + 748.98 / 1.02, 1e-6);
	CHECK_NEAR(slopes[1], 0.02 + 748.98 * 0.02 / 1.02, 1e-6);
	/* fixed shares when asked */
	config.proportional_slopes = false;
	CHECK_EQ(mp_net_port_slopes(&config, flows, 2, slopes), 0);
	CHECK_NEAR(slopes[0], 400, 1e-9);
	CHECK_NEAR(slopes[1], 350, 1e-9);
}

/*
 * TAS, one class, cycle 500 us, window 70 %: W = 350 - 1.136 = 348.864,
 * rate 697.728 bit/us, latency 151.136.
 *   source = 151.136 + 1136 / 697.728                    = 152.764143
 *   switch = 151.136 + (1136 + 1.136 x 152.764143) / 697.728
 *                                                        = 153.012869
 *   total  = 152.764143 + 2 + 153.012869                 = 307.777012
 */
static void test_time_aware_shaping(void)
{
	struct mp_net_config config = network;
	struct mp_flow flow = {
		.source_controller = MP_CONTROLLER_PCU,
		.destination_controller = MP_CONTROLLER_ZCU1,
		.message_bytes = 100, .period = 1000, .jitter = 0,
	};
	struct mp_network_summary summary;

	config.shaping = MP_NET_TAS;
	mp_net_analysis(&config, &flow, 1, &summary);
	CHECK(flow.network_ok);
	CHECK_NEAR(flow.source_delay, 152.764143, 1e-3);
	CHECK_NEAR(flow.total_delay, 307.777012, 1e-3);
}

/*
 * Tree: a ZCU1 -> ZCU2 flow crosses 4 ports, its burst growing at each
 * one; the bound is recomputed port by port here.
 */
static void test_tree_route_bound(void)
{
	struct mp_net_config config = network;
	struct mp_flow flow = {
		.source_controller = MP_CONTROLLER_ZCU1,
		.destination_controller = MP_CONTROLLER_ZCU2,
		.message_bytes = 100, .period = 1000, .jitter = 50,
	};
	struct mp_network_summary summary;
	double latency = cbs_latency(&network, 1136), rate = 1.136;
	double burst = 1136 + rate * 50, total = 0, delay;
	int h;

	config.topology = MP_NET_TREE;
	mp_net_analysis(&config, &flow, 1, &summary);
	for (h = 0; h < 4; h++) {
		delay = latency + burst / 750;
		total += delay + (h ? 2 : 0);
		burst += rate * delay;
	}
	CHECK(flow.network_ok);
	CHECK_EQ(flow.port_count, 4);
	CHECK_NEAR(flow.total_delay, total, 1e-3);
}

/*
 * Ring: flows from every controller to the one two switches further,
 * clockwise: the port dependencies form a cycle. The bounds are a fixed
 * point found by iteration; at a light load it exists.
 */
static void test_ring_fixed_point(void)
{
	struct mp_net_config config = network;
	struct mp_flow flows[6];
	struct mp_network_summary summary;
	int i;

	config.topology = MP_NET_RING;
	for (i = 0; i < 6; i++) {
		memset(&flows[i], 0, sizeof(flows[i]));
		flows[i].source_controller = i;
		flows[i].destination_controller = (i + 2) % 6;
		flows[i].message_bytes = 1500;
		flows[i].period = 1000;
		flows[i].jitter = 100;
	}
	mp_net_analysis(&config, flows, 6, &summary);
	CHECK(summary.iterations > 4);		/* not feed-forward */
	for (i = 0; i < 6; i++) {
		CHECK(flows[i].network_ok);
		CHECK_EQ(flows[i].port_count, 4);
		/* above the bounds without propagation */
		CHECK(flows[i].total_delay > 4 * cbs_latency(&network, 12336));
	}
	/* the same at 10 times the rate: no bound (port above its slope) */
	for (i = 0; i < 6; i++)
		flows[i].period = 15;
	mp_net_analysis(&config, flows, 6, &summary);
	CHECK(!flows[0].network_ok);
	CHECK(summary.overloaded_ports > 0);
}

/*
 * ATS: the interleaved regulators give every flow its source burst back
 * at each switch, so the four ports of a tree route see the same burst;
 * one class served at the link rate after a best-effort frame.
 */
static void test_ats_no_burst_growth(void)
{
	struct mp_net_config config = network;
	struct mp_flow flow = {
		.source_controller = MP_CONTROLLER_ZCU1,
		.destination_controller = MP_CONTROLLER_ZCU2,
		.message_bytes = 100, .period = 1000, .jitter = 50,
	};
	struct mp_network_summary summary;
	double burst = 1136 + 1.136 * 50;
	double delay = (12336 + burst) / 1000;

	config.topology = MP_NET_TREE;
	config.shaping = MP_NET_ATS;
	mp_net_analysis(&config, &flow, 1, &summary);
	CHECK(flow.network_ok);
	CHECK_EQ(flow.port_count, 4);
	CHECK_NEAR(flow.total_delay, 4 * delay + 3 * 2, 1e-3);
}

/*
 * ATS with two classes on one port: the low class waits for the burst of
 * the high class and is served at the rate it leaves; the high class
 * waits for one low-class frame at most.
 */
static void test_ats_two_classes(void)
{
	struct mp_net_config config = network;
	struct mp_flow flows[2] = {
		{ .source_controller = MP_CONTROLLER_PCU,
		  .destination_controller = MP_CONTROLLER_ZCU1,
		  .message_bytes = 1500, .period = 1000, .jitter = 0 },
		{ .source_controller = MP_CONTROLLER_PCU,
		  .destination_controller = MP_CONTROLLER_ZCU2,
		  .message_bytes = 1500, .period = 50000, .jitter = 0 },
	};
	struct mp_network_summary summary;
	double high_rate = 12336.0 / 1000, high, low;

	mp_net_set_classes(&config, 2);
	config.shaping = MP_NET_ATS;
	mp_net_analysis(&config, flows, 2, &summary);
	CHECK(flows[0].network_ok && flows[1].network_ok);
	CHECK_EQ(flows[0].traffic_class, 0);
	CHECK_EQ(flows[1].traffic_class, 1);
	/* first port (PCU egress), shared by both */
	high = (12336.0 + 12336) / 1000;
	low = (12336.0 + 12336 + 12336) / (1000 - high_rate);
	CHECK_NEAR(flows[0].source_delay, high, 1e-3);
	CHECK_NEAR(flows[1].source_delay, low, 1e-3);
}

/* ATS on a ring: no burst propagation, hence no cyclic dependency */
static void test_ats_ring(void)
{
	struct mp_net_config config = network;
	struct mp_flow flows[6];
	struct mp_network_summary summary;
	int i;

	config.topology = MP_NET_RING;
	config.shaping = MP_NET_ATS;
	for (i = 0; i < 6; i++) {
		memset(&flows[i], 0, sizeof(flows[i]));
		flows[i].source_controller = i;
		flows[i].destination_controller = (i + 2) % 6;
		flows[i].message_bytes = 1500;
		flows[i].period = 1000;
		flows[i].jitter = 100;
	}
	mp_net_analysis(&config, flows, 6, &summary);
	CHECK(summary.iterations <= 2);
	for (i = 0; i < 6; i++)
		CHECK(flows[i].network_ok);
}

/*
 * FRER on a ring: ZCU1 to ZCU2 is one switch clockwise, the second member
 * stream goes the five switches the other way; both share the first and
 * last ports, nothing else.
 */
static void test_frer_routes(void)
{
	struct mp_net_config config = network;
	int primary[MP_NET_MAX_ROUTE], second[MP_NET_MAX_ROUTE];
	int count, count2, h, g;

	config.topology = MP_NET_RING;
	CHECK_EQ(mp_net_secondary_route(&config, MP_CONTROLLER_ZCU1,
					MP_CONTROLLER_ZCU2, second), 0);
	config.frer = true;
	CHECK_EQ(mp_net_config_check(&config), 0);
	count = mp_net_route(&config, MP_CONTROLLER_ZCU1, MP_CONTROLLER_ZCU2,
			     primary);
	count2 = mp_net_secondary_route(&config, MP_CONTROLLER_ZCU1,
					MP_CONTROLLER_ZCU2, second);
	CHECK_EQ(count, 3);
	CHECK_EQ(count2, 7);
	CHECK_EQ(second[0], primary[0]);
	CHECK_EQ(second[count2 - 1], primary[count - 1]);
	for (h = 1; h + 1 < count; h++)
		for (g = 1; g + 1 < count2; g++)
			CHECK(primary[h] != second[g]);
	/* no disjoint route on a star or a tree */
	config.topology = MP_NET_STAR;
	CHECK(mp_net_config_check(&config) != 0);
}

/*
 * FRER bound with ATS and one class, one replicated flow of 1458 bytes
 * (12000 bits, r = 12 bits/us): every port but the last has the source
 * burst, (12336 + 12000) / 1000 = 24.336 us; the copies reach the
 * elimination after at most 6 ports and 6 switches (158.016 us) and at
 * least the 2 switch latencies of the short route (4 us): the last port
 * sees 12000 + 12 (158.016 - 4) bits. The bound is that of the long route.
 */
static void test_frer_bound(void)
{
	struct mp_net_config config = network;
	struct mp_flow flow = {
		.source_controller = MP_CONTROLLER_ZCU1,
		.destination_controller = MP_CONTROLLER_ZCU2,
		.message_bytes = 1458, .period = 1000, .jitter = 0,
		.replicated = true,
	};
	struct mp_network_summary summary;
	double port = (12336.0 + 12000) / 1000, last;

	config.topology = MP_NET_RING;
	config.shaping = MP_NET_ATS;
	config.frer = true;
	mp_net_analysis(&config, &flow, 1, &summary);
	CHECK(flow.network_ok && flow.frer);
	CHECK_EQ(summary.frer_flows, 1);
	last = (12336 + 12000 + 12 * (6 * port + 12 - 4)) / 1000;
	CHECK_NEAR(flow.total_delay, 6 * port + 12 + last, 1e-3);

	/* not replicated: the short route only */
	flow.replicated = false;
	mp_net_analysis(&config, &flow, 1, &summary);
	CHECK(!flow.frer);
	CHECK_NEAR(flow.total_delay, 3 * port + 4, 1e-3);

	/* CBS: the second route loads the other half of the ring */
	config.shaping = MP_NET_CBS;
	flow.replicated = true;
	mp_net_analysis(&config, &flow, 1, &summary);
	CHECK(flow.network_ok && flow.frer);
	CHECK(summary.max_backbone_utilization > 0);
}

int main(void)
{
	mp_net_config_default(&network);
	RUN_TEST(test_message_bits);
	RUN_TEST(test_single_flow_bound);
	RUN_TEST(test_frame_preemption);
	RUN_TEST(test_routes);
	RUN_TEST(test_two_cbs_classes);
	RUN_TEST(test_proportional_slopes);
	RUN_TEST(test_time_aware_shaping);
	RUN_TEST(test_tree_route_bound);
	RUN_TEST(test_ring_fixed_point);
	RUN_TEST(test_ats_no_burst_growth);
	RUN_TEST(test_ats_two_classes);
	RUN_TEST(test_ats_ring);
	RUN_TEST(test_frer_routes);
	RUN_TEST(test_frer_bound);
	RUN_TEST(test_jitter_and_sharing);
	RUN_TEST(test_overloaded_port);
	RUN_TEST(test_unknown_arrival);
	RUN_TEST(test_link_limit_and_rate);
	RUN_TEST(test_chain_path_and_latency);
	RUN_TEST(test_patterns_on_one_core);
	return unit_summary();
}
