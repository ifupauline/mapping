// SPDX-License-Identifier: Apache-2.0
/*
 * Export inter-controller flows and their Network Calculus bounds for the
 * frame-level simulator (tests/netsim/netsim.py).
 *
 *   net_export map   <sigma> <seed> <out.csv> [options]
 *	the flows of a mapping of a generated application (sigma as a
 *	fraction, e.g. 0.3)
 *   net_export heavy <target load %> <seed> <out.csv> [options]
 *	random flows up to a load of every port they cross
 *
 * options: --algorithm BFDU|WFDU|FFDU|CADU (FFDU), --topology star|tree|ring
 * (star), --classes 1|2|3 (1), --tas (time-aware shaping instead of CBS),
 * --ats (asynchronous traffic shaping), --frer (ring: the flows of
 * replicated task-chains, map, or every other flow, heavy, go both ways),
 * --replicas P (map: % of replicated task-chains, 20 with --frer)
 *
 * Output: a first line "#net key=value ..." with the network (shaping,
 * classes, shares, cycle, rates, switch latency), then the CSV
 * tc,hop,src,dst,bytes,T,J,class,route,route2,bound with the route as the
 * nodes crossed, "0>6>3" (nodes 0 to 5 are the controllers), and route2
 * the route of the second member stream with FRER (empty without).
 */
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mapping/generator.h"
#include "mapping/network.h"
#include "mapping/io.h"
#include "mapping/pipeline.h"

/*
 * the network line, then one "#slope from>to s0,s1,s2" line per link with
 * the idle slopes (bits per us) the analysis of @flows gives its port
 */
static void write_header(FILE *out, const struct mp_net_config *network,
			 const struct mp_flow *flows, int count)
{
	static double slopes[256 * MP_NET_MAX_CLASSES];
	int k, l, from, to;

	fprintf(out, "#net shaping=%s classes=%d link=%d backbone=%d "
		"switch=%d cycle=%d shares=",
		mp_net_shaping_name(network->shaping), network->class_count,
		network->link_mbps, network->backbone_mbps ?
		network->backbone_mbps : network->link_mbps,
		network->switch_latency, network->tas_cycle);
	for (k = 0; k < network->class_count; k++)
		fprintf(out, "%s%d", k ? "," : "",
			network->shaping == MP_NET_CBS ?
			network->idle_slope_percent[k] :
			network->tas_window_percent[k]);
	fprintf(out, "\n");
	/* the rate of every link, bits per us (= Mbit/s) */
	for (l = 0; l < mp_net_link_count(network); l++) {
		double rate = mp_net_link(network, l, &from, &to);

		fprintf(out, "#rate %d>%d %.9g\n", from, to, rate);
	}
	if (!mp_net_port_slopes(network, flows, count, slopes))
		for (l = 0; l < mp_net_link_count(network); l++) {
			mp_net_link(network, l, &from, &to);
			fprintf(out, "#slope %d>%d", from, to);
			for (k = 0; k < network->class_count; k++)
				fprintf(out, "%s%.9g", k ? "," : " ",
					slopes[l * MP_NET_MAX_CLASSES + k]);
			fprintf(out, "\n");
		}
	fprintf(out, "tc,hop,src,dst,bytes,T,J,class,route,route2,bound\n");
}

static void write_route(FILE *out, const struct mp_net_config *network,
			const int *links, int count)
{
	int h, from, to;

	for (h = 0; h < count; h++) {
		mp_net_link(network, links[h], &from, &to);
		if (!h)
			fprintf(out, "%d", from);
		fprintf(out, ">%d", to);
	}
}

static void write_flow(FILE *out, const struct mp_net_config *network,
		       int chain, int hop, const struct mp_flow *flow)
{
	int links[MP_NET_MAX_ROUTE], count;

	fprintf(out, "%d,%d,%d,%d,%d,%d,%d,%d,", chain, hop,
		flow->source_controller, flow->destination_controller,
		flow->message_bytes, flow->period, flow->jitter,
		flow->traffic_class);
	count = mp_net_route(network, flow->source_controller,
			     flow->destination_controller, links);
	write_route(out, network, links, count);
	fprintf(out, ",");
	if (flow->frer) {
		count = mp_net_secondary_route(network,
					       flow->source_controller,
					       flow->destination_controller,
					       links);
		write_route(out, network, links, count);
	}
	fprintf(out, ",%.4f\n", flow->network_ok ? flow->total_delay : -1.0);
}

/* the inter-controller hops of a feasible mapping */
static int export_mapping(FILE *out, int sigma, enum mp_algorithm algorithm,
			  const struct mp_params *base, int replicas,
			  uint64_t seed)
{
	const struct mp_net_config *network = &base->network;
	struct mp_design_point point;
	int p, h;

	for (;; seed++) {
		struct mp_chain_array application;
		struct mp_gen_params generator;
		struct mp_ctx ctx;
		int err;

		mp_ctx_init(&ctx, seed);
		ctx.params = *base;
		mp_gen_params_default(&generator);
		generator.replica_percent = replicas;
		if (mp_generate_application(&ctx, &generator, &application))
			return 1;
		ctx.params.sigma = sigma;
		ctx.params.algorithm = algorithm;
		err = mp_run_design_point(&ctx, &application, NULL, &point);
		mp_chain_array_free(&application);
		if (err)
			return 1;
		if (point.feasible)
			break;
		mp_design_point_free(&point);
	}
	{
		static struct mp_flow flows[65536];
		int count = 0;

		for (p = 0; p < point.paths.count; p++)
			for (h = 0; h < point.paths.items[p].hop_count; h++)
				if (point.paths.items[p].hops[h].type ==
				    MP_HOP_INTER_ECU && count < 65536)
					flows[count++] =
						point.paths.items[p].hops[h].flow;
		write_header(out, network, flows, count);
	}
	for (p = 0; p < point.paths.count; p++)
		for (h = 0; h < point.paths.items[p].hop_count; h++) {
			const struct mp_hop *hop = &point.paths.items[p].hops[h];

			if (hop->type == MP_HOP_INTER_ECU)
				write_flow(out, network,
					   point.paths.items[p].chain_id, h,
					   &hop->flow);
		}
	mp_design_point_free(&point);
	return 0;
}

/* random flows between the ECUs up to a load of every port */
static int export_heavy(FILE *out, double target_percent,
			const struct mp_net_config *network, uint64_t seed)
{
	static const int periods[] = {250, 500, 1000, 2000, 5000};
	static struct mp_flow flows[4096];
	double load[256] = {0};
	int last = mp_net_ecu_count(network) - 1;
	struct mp_network_summary summary;
	struct mp_rng rng;
	int count = 0, tries, i;

	mp_rng_seed(&rng, seed);
	for (tries = 0; tries < 20000 && count < 4096; tries++) {
		int links[2 * MP_NET_MAX_ROUTE], hops, h, largest, from, to;
		struct mp_flow flow;
		double rate;
		bool fits = true;

		memset(&flow, 0, sizeof(flow));
		flow.source_controller = mp_rng_range(&rng, 0, last);
		flow.destination_controller = mp_rng_range(&rng, 0, last);
		if (flow.source_controller == flow.destination_controller)
			continue;
		/* up to 3000 bytes: messages of several frames */
		flow.message_bytes = mp_rng_range(&rng, 64, 3000);
		flow.period = periods[mp_rng_range(&rng, 0, 4)];
		flow.jitter = mp_rng_range(&rng, 0, flow.period);
		flow.replicated = network->frer && count % 2;
		rate = (double)mp_net_message_bits(flow.message_bytes, &largest) /
		       flow.period;
		hops = mp_net_route(network, flow.source_controller,
				    flow.destination_controller, links);
		if (flow.replicated) {
			/* the ports of both member streams */
			int second[MP_NET_MAX_ROUTE], n, g;

			n = mp_net_secondary_route(network,
						   flow.source_controller,
						   flow.destination_controller,
						   second);
			for (g = 1; g + 1 < n; g++)
				links[hops++] = second[g];
		}
		/* the target is shared by the classes */
		for (h = 0; h < hops; h++)
			fits &= load[links[h]] + rate <= target_percent / 100.0 *
				mp_net_link(network, links[h], &from, &to) /
				network->class_count;
		if (!fits)
			continue;
		for (h = 0; h < hops; h++)
			load[links[h]] += rate;
		flows[count++] = flow;
	}
	mp_net_analysis(network, flows, count, &summary);
	write_header(out, network, flows, count);
	for (i = 0; i < count; i++)
		write_flow(out, network, i, 0, &flows[i]);
	return 0;
}

int main(int argc, char **argv)
{
	static const struct option options[] = {
		{"algorithm", required_argument, NULL, 'a'},
		{"topology", required_argument, NULL, 't'},
		{"classes", required_argument, NULL, 'c'},
		{"tas", no_argument, NULL, 's'},
		{"ats", no_argument, NULL, 'q'},
		{"frer", no_argument, NULL, 'f'},
		{"replicas", required_argument, NULL, 'r'},
		{"config", required_argument, NULL, 'g'},
		{NULL, 0, NULL, 0},
	};
	enum mp_algorithm algorithm = MP_FFDU;
	struct mp_params params;
	struct mp_net_config network;
	int option, status, replicas = -1;
	char error[400];
	FILE *out;

	mp_params_default(&params);
	mp_net_config_default(&network);
	while ((option = getopt_long(argc, argv, "", options, NULL)) != -1) {
		switch (option) {
		case 'a':
			if (mp_parse_algorithm(optarg, &algorithm))
				return 2;
			break;
		case 't':
			if (mp_net_parse_topology(optarg, &network.topology))
				return 2;
			break;
		case 'c':
			mp_net_set_classes(&network, atoi(optarg));
			break;
		case 's':
			network.shaping = MP_NET_TAS;
			break;
		case 'q':
			network.shaping = MP_NET_ATS;
			break;
		case 'f':
			network.frer = true;
			break;
		case 'r':
			replicas = atoi(optarg);
			break;
		case 'g':
			/* the platform and the network of a configuration */
			if (mp_read_configuration(optarg, &params, error,
						  sizeof(error))) {
				fprintf(stderr, "%s\n", error);
				return 2;
			}
			network = params.network;
			break;
		default:
			return 2;
		}
	}
	if (argc - optind != 4 ||
	    (strcmp(argv[optind], "map") && strcmp(argv[optind], "heavy"))) {
		fprintf(stderr, "usage: %s map <sigma> <seed> <out.csv> [options]\n"
			"       %s heavy <load %%> <seed> <out.csv> [options]\n"
			"options: --algorithm A --topology T --classes N --tas --ats "
			"--frer --replicas P --config FILE (first)\n",
			argv[0], argv[0]);
		return 2;
	}
	if (replicas < 0)
		replicas = network.frer ? 20 : 0;
	if (mp_net_config_check(&network)) {
		fprintf(stderr, "invalid network (--frer needs a ring or a "
			"custom topology)\n");
		return 2;
	}
	out = fopen(argv[optind + 3], "w");
	if (!out) {
		perror(argv[optind + 3]);
		return 2;
	}
	params.network = network;
	if (!strcmp(argv[optind], "map"))
		status = export_mapping(out, (int)(atof(argv[optind + 1]) *
						   MP_UTILIZATION_UNIT),
					algorithm, &params, replicas,
					(uint64_t)atoll(argv[optind + 2]));
	else
		status = export_heavy(out, atof(argv[optind + 1]), &network,
				      (uint64_t)atoll(argv[optind + 2]));
	fclose(out);
	return status;
}
