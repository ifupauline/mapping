// SPDX-License-Identifier: Apache-2.0
/*
 * experiment: the experiments of the methodology, run in parallel. Each
 * writes one CSV row per design point (plots.py draws the figures from it)
 * and prints a summary table of the means.
 *
 * Usage: experiment <name|all> [options]	(experiment --help)
 */
#define _GNU_SOURCE
#include <errno.h>
#include <getopt.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "mapping/experiment.h"
#include "mapping/io.h"
#include "mapping/table.h"

#define MAX_POINTS 256
#define MAX_COLUMNS 12

static const enum mp_algorithm algorithms[] = {MP_BFDU, MP_WFDU, MP_FFDU,
					       MP_CADU};

#define ALGORITHM_COUNT ((int)(sizeof(algorithms) / sizeof(algorithms[0])))

/* ---- summary columns ------------------------------------------------- */

/* one column of the summary table: the mean of a metric over a point */
struct column {
	const char *header;	/* name, '\n', unit */
	const char *format;	/* printf format of the mean */
	bool all_rows;		/* averaged over every row, not only the
				 * feasible ones */
	double (*value)(const struct mp_result_row *row);
};

#define METRIC(name, expression)					\
	static double name(const struct mp_result_row *row)		\
	{								\
		return (expression);					\
	}

METRIC(feasible_rate, row->feasible ? 100 : 0)
METRIC(cores_used, row->stats.used_cores)
METRIC(cores_pcu, row->stats.central_cores)
METRIC(cores_lower_bound, row->stats.lower_bound_cores)
METRIC(cores_zcu, row->stats.other_cores)
METRIC(sr_scheduling, row->stats.rate_scheduling)
METRIC(sr_final, row->stats.rate_final)
METRIC(chains_on_time, row->communication.on_time_chain_rate)
METRIC(deadline_misses, row->communication.deadline_misses)
METRIC(max_reaction, row->communication.max_reaction_time / 1e3)
METRIC(network_delay, row->communication.network.max_delay)
METRIC(port_load, row->communication.network.max_port_utilization)
METRIC(backbone_load, row->communication.network.max_backbone_utilization)
METRIC(frer_flows, row->communication.network.frer_flows)
METRIC(absorbed, row->lifecycle.absorbed)
METRIC(life_scaling, row->lifecycle.scaling[row->lifecycle.absorbed])
METRIC(life_on_time, row->lifecycle.on_time[row->lifecycle.absorbed])
METRIC(life_chains, row->lifecycle.chains[row->lifecycle.absorbed])
METRIC(inter_ecu_hops, row->communication.inter_ecu_hops)
METRIC(intra_ecu_hops, row->communication.intra_ecu_hops)
METRIC(let_load, row->stats.let_utilization)
METRIC(application_load, row->stats.application_utilization)
METRIC(fragmentation, row->stats.fragmentation_rate)
METRIC(scaling, row->stats.scaling_factor)
METRIC(repairs, row->stats.deadline_repairs)
METRIC(sized_phi, row->sizing.fill_limit / (double)MP_UTILIZATION_UNIT)
METRIC(target_met_rate, row->sizing.target_met ? 100 : 0)
METRIC(chain_count, row->chain_count)
METRIC(task_count, row->task_count)
METRIC(cpu_total, row->times.total)
METRIC(cpu_placement, row->times.swapping + row->times.displacement +
       row->times.repair)
/* CC: LET load over the capacity of the cores in use, % */
METRIC(let_overhead, row->stats.system_capacity > 0 ?
       100 * row->stats.let_utilization / row->stats.system_capacity : 0)

enum column_id {
	COL_FEASIBLE, COL_CORES, COL_PCU, COL_ZCU, COL_SR_SCHED, COL_SR_FINAL,
	COL_ON_TIME, COL_MISSES, COL_AGE, COL_NET, COL_PORT, COL_HOPS,
	COL_INTRA, COL_LET, COL_APP, COL_CC, COL_FR, COL_SCALING, COL_REPAIRS,
	COL_CHAINS, COL_TASKS, COL_CPU, COL_CPU_PLACE, COL_PHI, COL_MET,
	COL_LB, COL_BACKBONE, COL_FRER, COL_ABSORBED, COL_LIFE_SCALING,
	COL_LIFE_ON_TIME, COL_LIFE_CHAINS, COL_END = -1,
};

static const struct column columns[] = {
	[COL_FEASIBLE] = { "feasible\n%", "%.1f", true, feasible_rate },
	[COL_CORES] = { "cores\nM", "%.2f", false, cores_used },
	[COL_PCU] = { "PCU\ncores", "%.2f", false, cores_pcu },
	[COL_ZCU] = { "ZCU\ncores", "%.2f", false, cores_zcu },
	[COL_SR_SCHED] = { "SR sched\n%", "%.1f", false, sr_scheduling },
	[COL_SR_FINAL] = { "SR\n%", "%.1f", false, sr_final },
	[COL_ON_TIME] = { "on time\n% chains", "%.1f", false, chains_on_time },
	[COL_MISSES] = { "misses\nchains", "%.2f", false, deadline_misses },
	[COL_AGE] = { "reaction\nmax ms", "%.1f", false, max_reaction },
	[COL_NET] = { "net max\nus", "%.1f", false, network_delay },
	[COL_PORT] = { "port max\n%", "%.1f", false, port_load },
	[COL_HOPS] = { "net hops\ncount", "%.1f", false, inter_ecu_hops },
	[COL_INTRA] = { "ECU hops\ncount", "%.1f", false, intra_ecu_hops },
	[COL_LET] = { "LET load\ncores", "%.2f", false, let_load },
	[COL_APP] = { "app load\ncores", "%.2f", false, application_load },
	[COL_CC] = { "CC\n%", "%.1f", false, let_overhead },
	[COL_FR] = { "FR\n%", "%.1f", false, fragmentation },
	[COL_SCALING] = { "scaling\nfactor", "%.3f", false, scaling },
	[COL_REPAIRS] = { "repairs\nmoves", "%.2f", false, repairs },
	[COL_CHAINS] = { "chains\ncount", "%.0f", true, chain_count },
	[COL_TASKS] = { "tasks\ncount", "%.0f", true, task_count },
	[COL_CPU] = { "CPU\ns", "%.3f", true, cpu_total },
	[COL_CPU_PLACE] = { "placement\ns", "%.3f", true, cpu_placement },
	[COL_PHI] = { "phi\nsized", "%.3f", false, sized_phi },
	[COL_MET] = { "target\n% met", "%.1f", true, target_met_rate },
	[COL_LB] = { "lower\nbound", "%.2f", false, cores_lower_bound },
	[COL_BACKBONE] = { "backbone\nmax %", "%.2f", false, backbone_load },
	[COL_FRER] = { "FRER\nflows", "%.1f", false, frer_flows },
	[COL_ABSORBED] = { "updates\nabsorbed", "%.2f", false, absorbed },
	[COL_LIFE_CHAINS] = { "chains\nat end", "%.1f", false, life_chains },
	[COL_LIFE_SCALING] = { "scaling\nat end", "%.3f", false,
			       life_scaling },
	[COL_LIFE_ON_TIME] = { "on time\nat end %", "%.1f", false,
			       life_on_time },
};

/* ---- experiments ------------------------------------------------------- */

struct experiment_def {
	const char *name;
	const char *description;
	int default_instances;
	/* fills the point list, returns its length */
	int (*make_points)(struct mp_point_spec *points);
	const enum column_id columns[MAX_COLUMNS];
};

static struct mp_point_spec *add_point(struct mp_point_spec *points,
				       int *count, enum mp_algorithm algorithm,
				       const char *format, ...)
	__attribute__((format(printf, 4, 5)));

/* a point with a label and an algorithm, every other value by default */
static struct mp_point_spec *add_point(struct mp_point_spec *points,
				       int *count, enum mp_algorithm algorithm,
				       const char *format, ...)
{
	struct mp_point_spec *point = &points[(*count)++];
	va_list args;

	mp_point_spec_default(point);
	point->params.algorithm = algorithm;
	va_start(args, format);
	vsnprintf(point->label, sizeof(point->label), format, args);
	va_end(args);
	return point;
}

/*
 * partitioning limit x bin packing: most chains use less than 8 % of a
 * core, so the small values are the ones that cut
 */
static int sweep_points(struct mp_point_spec *points)
{
	static const int sigmas[] = {20000, 30000, 50000, 100000, 200000,
				     400000, 800000};
	int count = 0, s, a;

	for (s = 0; s < 7; s++)
		for (a = 0; a < ALGORITHM_COUNT; a++)
			add_point(points, &count, algorithms[a], "sigma=%.2f",
				  sigmas[s] / 1e6)->params.sigma = sigmas[s];
	return count;
}

/* run time against the size of the application */
static int scaling_points(struct mp_point_spec *points)
{
	int count = 0, chains, a;

	for (chains = 50; chains <= 3200; chains *= 2)
		for (a = 0; a < ALGORITHM_COUNT; a++)
			add_point(points, &count, algorithms[a], "chains=%d",
				  chains)->generator.chain_count = chains;
	return count;
}

/* speed of the zone controllers against the central computer */
static int architecture_points(struct mp_point_spec *points)
{
	static const int speeds[] = {25, 50, 75, 100};
	int count = 0, s, a;

	for (s = 0; s < 4; s++)
		for (a = 0; a < ALGORITHM_COUNT; a++)
			add_point(points, &count, algorithms[a],
				  "ZCU speed=%d%%", speeds[s])->params.zcu_speed =
				speeds[s];
	return count;
}

/* assignment with and without affinity, for three partitioning limits */
static int affinity_points(struct mp_point_spec *points)
{
	int count = 0, sigma, affinity, a;

	for (sigma = 200000; sigma <= 600000; sigma += 200000)
		for (affinity = 0; affinity < 2; affinity++)
			for (a = 0; a < ALGORITHM_COUNT; a++) {
				struct mp_point_spec *point;

				point = add_point(points, &count, algorithms[a],
						  "sigma=%.1f %s", sigma / 1e6,
						  affinity ? "affinity" :
						  "no affinity");
				point->params.sigma = sigma;
				point->params.affinity = affinity;
			}
	return count;
}

/* link rate x frame preemption x message sizes */
static int network_points(struct mp_point_spec *points)
{
	static const int rates[] = {100, 1000};
	int count = 0, r, preemption, heavy, a;

	for (heavy = 0; heavy < 2; heavy++)
		for (r = 0; r < 2; r++)
			for (preemption = 0; preemption < 2; preemption++)
				for (a = 0; a < ALGORITHM_COUNT; a++) {
					struct mp_point_spec *point;

					point = add_point(points, &count,
						algorithms[a], "%dM %s%s",
						rates[r], heavy ? "sensors" :
						"default", preemption ?
						" preempt" : "");
					point->params.network.link_mbps = rates[r];
					point->params.network.preemption =
						preemption;
					if (!heavy)
						continue;
					/* a third of the messages are large */
					point->generator.small_message_percent = 40;
					point->generator.medium_message_percent = 30;
					point->generator.large_message_bytes = 64000;
				}
	return count;
}

/* the three topologies, backbone at 1 and 10 Gbit/s */
static int topology_points(struct mp_point_spec *points)
{
	static const enum mp_net_topology topologies[] = {
		MP_NET_STAR, MP_NET_TREE, MP_NET_RING,
	};
	int count = 0, t, fast, a;

	for (t = 0; t < 3; t++)
		for (fast = 0; fast < 2; fast++) {
			/* a star has no backbone */
			if (topologies[t] == MP_NET_STAR && fast)
				continue;
			for (a = 0; a < ALGORITHM_COUNT; a++) {
				struct mp_point_spec *point;

				point = add_point(points, &count, algorithms[a],
						  "%s%s",
						  mp_net_topology_name(topologies[t]),
						  fast ? " 10G" : "");
				point->params.network.topology = topologies[t];
				point->params.network.backbone_mbps =
					fast ? 10000 : 0;
			}
		}
	/* asynchronous traffic shaping: no burst accumulation on the ring */
	for (t = 1; t < 3; t++)
		for (a = 0; a < ALGORITHM_COUNT; a++) {
			struct mp_point_spec *point;

			point = add_point(points, &count, algorithms[a],
					  "%s ATS",
					  mp_net_topology_name(topologies[t]));
			point->params.network.topology = topologies[t];
			point->params.network.shaping = MP_NET_ATS;
		}
	return count;
}

/*
 * traffic classes on the whole vehicle (2000 chains, 16 PCU cores): one
 * CBS class at 1 Gbit/s as in version 2.0, then with a 10 Gbit/s
 * backbone (and PCU link): one to three CBS classes by period, or
 * time-aware shaping
 */
static int classes_points(struct mp_point_spec *points)
{
	static const struct {
		int classes;
		enum mp_net_shaping shaping;
		int backbone;
	} configs[] = {
		{1, MP_NET_CBS, 0}, {1, MP_NET_CBS, 10000},
		{2, MP_NET_CBS, 10000}, {3, MP_NET_CBS, 10000},
		{1, MP_NET_TAS, 10000}, {3, MP_NET_TAS, 10000},
		{1, MP_NET_ATS, 10000}, {3, MP_NET_ATS, 10000},
	};
	int count = 0, c, a;

	for (c = 0; c < 8; c++)
		for (a = 0; a < ALGORITHM_COUNT; a++) {
			struct mp_point_spec *point;

			point = add_point(points, &count, algorithms[a],
					  "%d %s%s", configs[c].classes,
					  mp_net_shaping_name(configs[c].shaping),
					  configs[c].backbone ? " 10G" : " 1G");
			point->generator.chain_count = 2000;
			point->params.pcu_cores = 16;
			mp_net_set_classes(&point->params.network,
					   configs[c].classes);
			point->params.network.shaping = configs[c].shaping;
			point->params.network.backbone_mbps =
				configs[c].backbone;
		}
	return count;
}

/*
 * spatial constraints: replicated safety chains (on other controllers,
 * or only other cores), zone-only chains, co-located tasks, memory of
 * the zone controllers
 */
static int constraints_points(struct mp_point_spec *points)
{
	static const struct {
		const char *label;
		int replicas, cores, zone_only, colocate, memory;
	} configs[] = {
		{"none", 0, 0, 0, 0, 0},
		{"replicas 10%", 10, 0, 0, 0, 0},
		{"replicas 20%", 20, 0, 0, 0, 0},
		{"replicas 30%", 30, 0, 0, 0, 0},
		{"replicas 20% cores", 20, 1, 0, 0, 0},
		{"zone-only 20%", 0, 0, 20, 0, 0},
		{"co-located 30%", 0, 0, 0, 30, 0},
		{"ZCU memory 8 MB", 0, 0, 0, 0, 8192},
	};
	int count = 0, c, a;

	for (c = 0; c < 8; c++)
		for (a = 0; a < ALGORITHM_COUNT; a++) {
			struct mp_point_spec *point;

			point = add_point(points, &count, algorithms[a], "%s",
					  configs[c].label);
			point->generator.replica_percent = configs[c].replicas;
			point->generator.zone_only_percent = configs[c].zone_only;
			point->generator.colocate_percent = configs[c].colocate;
			point->params.zcu_memory = configs[c].memory;
			if (configs[c].cores)
				point->params.separation = MP_SEPARATE_CORES;
		}
	return count;
}

/*
 * frame replication (802.1CB) on the ring for the messages of replicated
 * task-chains, with credit-based or asynchronous traffic shaping
 */
static int frer_points(struct mp_point_spec *points)
{
	static const int replicas[] = {0, 10, 20, 30};
	int count = 0, ats, r, frer, a;

	for (ats = 0; ats < 2; ats++)
		for (r = 0; r < 4; r++)
			for (frer = 0; frer < 2; frer++) {
				if (!replicas[r] && frer)
					continue;
				for (a = 0; a < ALGORITHM_COUNT; a++) {
					struct mp_point_spec *point;

					point = add_point(points, &count,
							  algorithms[a],
							  "%s replicas %d%%%s",
							  ats ? "ATS" : "CBS",
							  replicas[r],
							  frer ? " FRER" : "");
					point->generator.replica_percent =
						replicas[r];
					point->params.network.topology =
						MP_NET_RING;
					point->params.network.shaping =
						ats ? MP_NET_ATS : MP_NET_CBS;
					point->params.network.frer = frer;
				}
			}
	return count;
}

/*
 * types of cores, every WCET up to 30 % off its speed ratio on each type:
 * the PCU with big cores only or half little ones, the zone controllers
 * with microcontroller cores only or half lock-step ones running the
 * safety chains (10 %)
 */
static int cores_points(struct mp_point_spec *points)
{
	static const struct {
		const char *label;
		int big, little, mcu, lockstep, safety;
	} configs[] = {
		{"8 big | 4 MCU", 8, 0, 4, 0, 0},
		{"4 big + 8 little | 4 MCU", 4, 8, 4, 0, 0},
		{"8 big | 2 MCU + 2 LS", 8, 0, 2, 2, 10},
		{"4 big + 8 little | 2 MCU + 2 LS", 4, 8, 2, 2, 10},
	};
	int count = 0, c, a;

	for (c = 0; c < 4; c++)
		for (a = 0; a < ALGORITHM_COUNT; a++) {
			struct mp_point_spec *point;

			point = add_point(points, &count, algorithms[a], "%s",
					  configs[c].label);
			point->params.pcu_cores = configs[c].big;
			point->params.pcu_little_cores = configs[c].little;
			point->params.zcu_cores = configs[c].mcu;
			point->params.zcu_lockstep_cores = configs[c].lockstep;
			point->generator.type_variation = 30;
			point->generator.lockstep_percent = configs[c].safety;
		}
	return count;
}

/*
 * filling limit phi: above about 70 % the cores may no longer be
 * schedulable (non-harmonic periods), the placement phases then work
 */
static int fill_points(struct mp_point_spec *points)
{
	static const int limits[] = {600000, 700000, 800000, 900000, 1000000};
	int count = 0, f, a;

	for (f = 0; f < 5; f++)
		for (a = 0; a < ALGORITHM_COUNT; a++)
			add_point(points, &count, algorithms[a], "phi=%.1f",
				  limits[f] / 1e6)->params.fill_limit =
				limits[f];
	return count;
}

/* WCET of the LET tasks */
static int let_points(struct mp_point_spec *points)
{
	static const int let_wcets[] = {10, 50, 100, 200};
	int count = 0, l, a;

	for (l = 0; l < 4; l++)
		for (a = 0; a < ALGORITHM_COUNT; a++)
			add_point(points, &count, algorithms[a], "LET=%dus",
				  let_wcets[l])->params.let_wcet = let_wcets[l];
	return count;
}

/*
 * life of a platform: sized at the SOP for 300 task-chains with a filling
 * limit phi, then ten software updates of 15 task-chains each (+50 %) on
 * that hardware, up to phi = 0.8
 */
static int lifecycle_points(struct mp_point_spec *points)
{
	static const int limits[] = {500000, 600000, 700000, 800000};
	int count = 0, l, a;

	for (l = 0; l < 4; l++)
		for (a = 0; a < ALGORITHM_COUNT; a++) {
			struct mp_point_spec *point;

			point = add_point(points, &count, algorithms[a],
					  "SOP phi=%.1f", limits[l] / 1e6);
			point->params.fill_limit = limits[l];
			point->lifecycle.updates = 10;
		}
	return count;
}

/*
 * shared-resource contention: a shared memory per controller served in
 * round robin, 1000 requests per ms of WCET, latency of a request
 */
static int contention_points(struct mp_point_spec *points)
{
	static const int latencies[] = {0, 20, 50, 100};
	int count = 0, l, a;

	for (l = 0; l < 4; l++)
		for (a = 0; a < ALGORITHM_COUNT; a++)
			add_point(points, &count, algorithms[a], "%d ns",
				  latencies[l])->params.memory_latency =
				latencies[l];
	return count;
}

/*
 * the sub-chain limit sigma where it matters: a sub-chain must fit a core
 * with its LET task and the contention, so whole task-chains (sigma = phi)
 * can make a mapping infeasible
 */
static int limit_points(struct mp_point_spec *points)
{
	static const enum mp_algorithm rules[] = {MP_FFDU, MP_CADU};
	static const int sigmas[] = {400000, 600000, 800000};
	static const char *const settings[] = {
		"default", "contention 50 ns", "600 chains",
	};
	int count = 0, setting, s, a;

	for (setting = 0; setting < 3; setting++)
		for (s = 0; s < 3; s++)
			for (a = 0; a < 2; a++) {
				struct mp_point_spec *point;

				point = add_point(points, &count, rules[a],
						  "%s sigma=%.1f",
						  settings[setting],
						  sigmas[s] / 1e6);
				point->params.sigma = sigmas[s];
				if (setting == 1)
					point->params.memory_latency = 50;
				if (setting == 2)
					point->generator.chain_count = 600;
			}
	return count;
}

/*
 * communication between cores: implicit (an output copied at the first
 * LET instant after its producer ends) or strict LET (published at the
 * end of the producer's period), on the default point, with loose
 * deadlines and on a ring
 */
static int communication_points(struct mp_point_spec *points)
{
	static const enum mp_algorithm rules[] = {MP_FFDU, MP_CADU};
	static const char *const settings[] = {
		"default", "deadline 500%", "ring 3 classes",
	};
	int count = 0, setting, strict, a;

	for (setting = 0; setting < 3; setting++)
		for (strict = 0; strict < 2; strict++)
			for (a = 0; a < 2; a++) {
				struct mp_point_spec *point;

				point = add_point(points, &count, rules[a],
						  "%s %s", settings[setting],
						  strict ? "LET" : "implicit");
				point->params.network.strict_let = strict;
				if (setting == 1)
					point->generator.deadline_percent = 500;
				if (setting == 2) {
					point->params.network.topology =
						MP_NET_RING;
					mp_net_set_classes(&point->params.network,
							   3);
				}
			}
	return count;
}

/*
 * two ways to a margin: WCETs increased at design time, or phi lowered
 * until the mapping reaches a WCET scaling factor (sizing)
 */
static int margins_points(struct mp_point_spec *points)
{
	static const int margins[] = {0, 10, 20, 40};
	static const int targets[] = {1100, 1250, 1500};
	int count = 0, m, a;

	for (m = 0; m < 4; m++)
		for (a = 0; a < ALGORITHM_COUNT; a++)
			add_point(points, &count, algorithms[a], "margin=%d%%",
				  margins[m])->params.wcet_margin = margins[m];
	for (m = 0; m < 3; m++)
		for (a = 0; a < ALGORITHM_COUNT; a++)
			add_point(points, &count, algorithms[a],
				  "target=%.2f", targets[m] / 1000.0)
				->params.target_scaling = targets[m];
	return count;
}

/* end-to-end deadlines x deadline repair */
static int deadlines_points(struct mp_point_spec *points)
{
	static const int deadlines[] = {200, 300, 400, 600};
	int count = 0, d, repair, a;

	for (d = 0; d < 4; d++)
		for (repair = 0; repair < 2; repair++)
			for (a = 0; a < ALGORITHM_COUNT; a++) {
				struct mp_point_spec *point;

				point = add_point(points, &count, algorithms[a],
						  "D=%d%% %s", deadlines[d],
						  repair ? "repair" :
						  "no repair");
				point->generator.deadline_percent =
					deadlines[d];
				point->params.repair_deadlines = repair;
			}
	return count;
}

/*
 * ablation: the method with one part removed at a time (and with the
 * optional swap moves added), then with all of them removed (bin packing
 * and the optimal priority assignment only), on
 * the default setting and on a stressed one (cores filled up to 100 %,
 * deadlines at 200 % of the pattern periods), where the placement moves
 * and the local search have work to do
 */
static int ablation_points(struct mp_point_spec *points)
{
	static const enum mp_algorithm rules[] = {MP_FFDU, MP_CADU};
	static const char *const variants[] = {
		"full", "no affinity", "no consolidation", "with swap moves",
		"no relocation moves", "no local search", "none",
	};
	int count = 0, setting, v, a;

	for (setting = 0; setting < 2; setting++)
		for (v = 0; v < 7; v++)
			for (a = 0; a < 2; a++) {
				struct mp_point_spec *point;
				struct mp_params *params;

				point = add_point(points, &count, rules[a], "%s %s",
						  setting ? "stressed" : "default",
						  variants[v]);
				params = &point->params;
				if (setting) {
					params->fill_limit = MP_UTILIZATION_UNIT;
					point->generator.deadline_percent = 200;
				}
				params->affinity = v != 1 && v != 6;
				params->consolidate = v != 2 && v != 6;
				params->swap_moves = v == 3;
				params->relocation_moves = v != 4 && v != 6;
				params->repair_deadlines = v != 5 && v != 6;
			}
	return count;
}

/*
 * design-space exploration: the same functions (generator seeds) on the
 * four E/E architectures of the templates, at three sizes; the sensors
 * and actuators are wired to the ECUs that are not central computers
 */
static int dse_points(struct mp_point_spec *points)
{
	static const char *const templates[] = {
		"distributed", "domain", "hybrid", "zonal",
	};
	static const enum mp_algorithm rules[] = {MP_FFDU, MP_CADU};
	static const int sizes[] = {100, 200, 300};
	int count = 0, t, z, a;

	for (t = 0; t < 4; t++)
		for (z = 0; z < 3; z++)
			for (a = 0; a < 2; a++) {
				struct mp_point_spec *point;
				enum mp_algorithm rule = rules[a];

				point = add_point(points, &count, rule, "%s %d",
						  templates[t], sizes[z]);
				mp_params_template(&point->params, templates[t]);
				point->params.algorithm = rule;
				point->generator.chain_count = sizes[z];
			}
	return count;
}

/*
 * robustness to the assumptions of the generator: one at a time, a lower
 * and a higher value than the default
 */
static int robustness_points(struct mp_point_spec *points)
{
	static const enum mp_algorithm rules[] = {MP_FFDU, MP_CADU};
	int count = 0, level, a;

	for (a = 0; a < 2; a++) {
		add_point(points, &count, rules[a], "default");
		for (level = 0; level < 2; level++) {
			struct mp_gen_params *g;

			g = &add_point(points, &count, rules[a], "local %d%%",
				       level ? 50 : 0)->generator;
			g->local_percent = level ? 50 : 0;
			g = &add_point(points, &count, rules[a], "sensors %d%%",
				       level ? 90 : 30)->generator;
			g->sensor_percent = level ? 90 : 30;
			g->actuator_percent = level ? 80 : 25;
			g = &add_point(points, &count, rules[a], "chain load x%s",
				       level ? "2" : "0.5")->generator;
			g->max_chain_utilization = level ? 160000 : 40000;
			g = &add_point(points, &count, rules[a], "heavy %d%%",
				       level ? 5 : 0)->generator;
			g->heavy_percent = level ? 5 : 0;
			g = &add_point(points, &count, rules[a], "messages %s",
				       level ? "large" : "small")->generator;
			g->small_message_percent = level ? 40 : 90;
			g->medium_message_percent = level ? 40 : 10;
		}
	}
	return count;
}

/*
 * the method against what a practitioner would do: keep the chains whole
 * (cut only where the ECUs force it), put the free software on the
 * central computer, or balance the load (worst fit)
 */
static int practice_points(struct mp_point_spec *points)
{
	int count = 0;

	add_point(points, &count, MP_FFDU, "method FFDU");
	add_point(points, &count, MP_CADU, "method CADU");
	add_point(points, &count, MP_FFDU, "whole chains")->params.sigma =
		MP_FILL_LIMIT;
	add_point(points, &count, MP_FFDU, "central only")->params.central_only =
		true;
	add_point(points, &count, MP_FFDU, "whole + central");
	points[count - 1].params.sigma = MP_FILL_LIMIT;
	points[count - 1].params.central_only = true;
	add_point(points, &count, MP_WFDU, "load balancing");
	return count;
}

/* the detection chain at phi = 1 and 0.8, sigma up to phi */
static int detection_points(struct mp_point_spec *points)
{
	static const int fill_limits[] = {1000000, 800000};
	int count = 0, f, sigma, a;

	for (f = 0; f < 2; f++)
		for (sigma = 200000; sigma <= fill_limits[f]; sigma += 100000)
			for (a = 0; a < ALGORITHM_COUNT; a++) {
				struct mp_point_spec *point;

				point = add_point(points, &count, algorithms[a],
						  "phi=%.1f sigma=%.1f",
						  fill_limits[f] / 1e6,
						  sigma / 1e6);
				point->application = MP_APPLICATION_DETECTION;
				point->params.fill_limit = fill_limits[f];
				point->params.sigma = sigma;
				/* the challenge platform: identical cores */
				point->params.zcu_speed = 100;
			}
	return count;
}

/*
 * The whole software of a vehicle: 2000 task-chains (about 8700 tasks),
 * on a central computer of 16 cores and five zone controllers of 4. The
 * chain utilizations are those of automotive microcontroller cores: with
 * the reference speeds the PCU cores are such cores; with "fast PCU" they
 * are application-class cores 4 times faster, and the zone controllers
 * keep the microcontroller speed.
 */
static int vehicle_points(struct mp_point_spec *points)
{
	int count = 0, fast, a;

	for (fast = 0; fast < 2; fast++)
		for (a = 0; a < ALGORITHM_COUNT; a++) {
			struct mp_point_spec *point;

			point = add_point(points, &count, algorithms[a], "%s",
					  fast ? "fast PCU" : "reference");
			point->generator.chain_count = 2000;
			point->params.pcu_cores = 16;
			if (fast) {
				point->params.pcu_speed = 400;
				point->params.zcu_speed = 100;
			}
		}
	return count;
}

static const struct experiment_def experiments[] = {
	{ "sweep", "partitioning limit sigma x assignment algorithm",
	  20, sweep_points,
	  { COL_FEASIBLE, COL_CORES, COL_LB, COL_PCU, COL_ZCU, COL_FR,
	    COL_SR_SCHED, COL_SR_FINAL, COL_ON_TIME, COL_AGE, COL_CPU,
	    COL_END } },
	{ "scaling", "CPU time against the number of task-chains",
	  2, scaling_points,
	  { COL_CHAINS, COL_TASKS, COL_FEASIBLE, COL_CORES, COL_SR_FINAL,
	    COL_CPU_PLACE, COL_CPU, COL_END } },
	{ "architecture", "speed of the zone controllers (PCU at 100 %)",
	  20, architecture_points,
	  { COL_FEASIBLE, COL_CORES, COL_LB, COL_PCU, COL_ZCU, COL_SR_FINAL,
	    COL_ON_TIME, COL_LET, COL_END } },
	{ "affinity", "assignment with and without affinity",
	  20, affinity_points,
	  { COL_FEASIBLE, COL_CORES, COL_INTRA, COL_HOPS, COL_SR_FINAL,
	    COL_ON_TIME, COL_AGE, COL_END } },
	{ "network", "link rate x frame preemption x message sizes",
	  20, network_points,
	  { COL_FEASIBLE, COL_CORES, COL_HOPS, COL_NET, COL_PORT, COL_ON_TIME,
	    COL_AGE, COL_END } },
	{ "topology", "network topology: star, tree, ring",
	  20, topology_points,
	  { COL_FEASIBLE, COL_CORES, COL_HOPS, COL_NET, COL_PORT, COL_ON_TIME,
	    COL_AGE, COL_END } },
	{ "classes", "traffic classes and shaping on the whole vehicle",
	  2, classes_points,
	  { COL_FEASIBLE, COL_CORES, COL_NET, COL_PORT, COL_ON_TIME,
	    COL_MISSES, COL_CPU, COL_END } },
	{ "constraints", "spatial constraints: replicas, zones, co-location, "
	  "memory", 20, constraints_points,
	  { COL_FEASIBLE, COL_CORES, COL_PCU, COL_ZCU, COL_HOPS, COL_ON_TIME,
	    COL_AGE, COL_END } },
	{ "frer", "frame replication (802.1CB) on the ring for replicated "
	  "task-chains", 20, frer_points,
	  { COL_FEASIBLE, COL_CORES, COL_HOPS, COL_FRER, COL_NET, COL_BACKBONE,
	    COL_ON_TIME, COL_AGE, COL_END } },
	{ "cores", "types of cores: big, little, microcontroller, lock-step",
	  20, cores_points,
	  { COL_FEASIBLE, COL_CORES, COL_PCU, COL_ZCU, COL_APP, COL_SCALING,
	    COL_ON_TIME, COL_END } },
	{ "fill", "filling limit phi of the cores",
	  20, fill_points,
	  { COL_FEASIBLE, COL_CORES, COL_SR_SCHED, COL_SR_FINAL, COL_SCALING,
	    COL_ON_TIME, COL_CPU_PLACE, COL_END } },
	{ "let", "WCET of the LET tasks",
	  20, let_points,
	  { COL_FEASIBLE, COL_CORES, COL_LET, COL_CC, COL_SR_FINAL,
	    COL_ON_TIME, COL_END } },
	{ "contention", "shared memory per controller: latency of a request",
	  20, contention_points,
	  { COL_FEASIBLE, COL_CORES, COL_PCU, COL_ZCU, COL_APP, COL_SR_FINAL,
	    COL_SCALING, COL_ON_TIME, COL_END } },
	{ "lifecycle", "platform sized at the SOP, then software updates",
	  20, lifecycle_points,
	  { COL_FEASIBLE, COL_CORES, COL_SCALING, COL_ABSORBED,
	    COL_LIFE_CHAINS, COL_LIFE_SCALING, COL_LIFE_ON_TIME, COL_END } },
	{ "margins", "WCET margin at design time, or sizing for a scaling factor",
	  20, margins_points,
	  { COL_FEASIBLE, COL_CORES, COL_APP, COL_SR_FINAL, COL_SCALING,
	    COL_PHI, COL_MET, COL_ON_TIME, COL_END } },
	{ "deadlines", "end-to-end deadlines x deadline repair",
	  20, deadlines_points,
	  { COL_FEASIBLE, COL_CORES, COL_ON_TIME, COL_MISSES, COL_REPAIRS,
	    COL_AGE, COL_CPU, COL_END } },
	{ "limit", "sub-chain limit sigma: default, contention, 600 chains",
	  20, limit_points,
	  { COL_FEASIBLE, COL_CORES, COL_LB, COL_HOPS, COL_ON_TIME, COL_CPU,
	    COL_END } },
	{ "communication", "implicit communication or strict LET between "
	  "cores", 20, communication_points,
	  { COL_FEASIBLE, COL_CORES, COL_HOPS, COL_NET, COL_AGE, COL_ON_TIME,
	    COL_END } },
	{ "ablation", "the method with each part removed, default and "
	  "stressed", 20, ablation_points,
	  { COL_FEASIBLE, COL_CORES, COL_SR_SCHED, COL_SR_FINAL, COL_HOPS,
	    COL_ON_TIME, COL_MISSES, COL_CPU, COL_END } },
	{ "dse", "the same functions on four E/E architectures",
	  20, dse_points,
	  { COL_FEASIBLE, COL_CORES, COL_LB, COL_HOPS, COL_PORT, COL_ON_TIME,
	    COL_AGE, COL_CPU, COL_END } },
	{ "robustness", "assumptions of the generator, one at a time",
	  20, robustness_points,
	  { COL_FEASIBLE, COL_CORES, COL_LB, COL_HOPS, COL_PORT, COL_ON_TIME,
	    COL_AGE, COL_END } },
	{ "practice", "the method against practitioner rules", 20,
	  practice_points,
	  { COL_FEASIBLE, COL_CORES, COL_LB, COL_PCU, COL_ZCU, COL_HOPS,
	    COL_PORT, COL_ON_TIME, COL_AGE, COL_END } },
	{ "detection", "the detection chain (9 tasks) at phi = 1 and 0.8",
	  1, detection_points,
	  { COL_FEASIBLE, COL_CORES, COL_SR_FINAL, COL_CC, COL_FR,
	    COL_SCALING, COL_END } },
	{ "vehicle", "case study: the software of a whole vehicle",
	  2, vehicle_points,
	  { COL_TASKS, COL_FEASIBLE, COL_CORES, COL_LB, COL_PCU, COL_ZCU,
	    COL_SR_FINAL, COL_ON_TIME, COL_PORT, COL_CPU, COL_END } },
};

#define EXPERIMENT_COUNT ((int)(sizeof(experiments) / sizeof(experiments[0])))

/* ---- summary ----------------------------------------------------------- */

/* mean of every column over the rows of each point (NAN: no row) */
static void summarize(const struct experiment_def *definition,
		      const struct mp_result_row *rows, int count,
		      int point_count, double (*means)[MAX_COLUMNS])
{
	static int counts[MAX_POINTS][MAX_COLUMNS];
	int i, c, p;

	memset(counts, 0, sizeof(counts));
	memset(means, 0, (size_t)point_count * sizeof(*means));
	for (i = 0; i < count; i++) {
		const struct mp_result_row *row = &rows[i];

		if (row->error)
			continue;
		for (c = 0; definition->columns[c] != COL_END; c++) {
			const struct column *column =
				&columns[definition->columns[c]];

			if (!column->all_rows && !row->feasible)
				continue;
			means[row->point][c] += column->value(row);
			counts[row->point][c]++;
		}
	}
	for (p = 0; p < point_count; p++)
		for (c = 0; definition->columns[c] != COL_END; c++)
			means[p][c] = counts[p][c] ? means[p][c] / counts[p][c] :
				      NAN;
}

static void print_summary(const struct experiment_def *definition,
			  const struct mp_point_spec *points, int point_count,
			  double (*means)[MAX_COLUMNS])
{
	struct mp_table *table;
	int column_count = 0, p, c;

	while (definition->columns[column_count] != COL_END)
		column_count++;
	table = mp_table_new(column_count + 2);
	mp_table_header(table, 0, "point", false);
	mp_table_header(table, 1, "alpha", false);
	for (c = 0; c < column_count; c++)
		mp_table_header(table, c + 2,
				columns[definition->columns[c]].header, true);

	for (p = 0; p < point_count; p++) {
		/* a rule between groups of the algorithms */
		if (p && points[p].params.algorithm == MP_BFDU)
			mp_table_rule(table);
		mp_table_row(table);
		mp_table_cell(table, 0, "%s", points[p].label);
		mp_table_cell(table, 1, "%s",
			      mp_algorithm_name(points[p].params.algorithm));
		for (c = 0; c < column_count; c++) {
			char text[32];

			if (isnan(means[p][c])) {
				mp_table_cell(table, c + 2, "-");
				continue;
			}
			snprintf(text, sizeof(text),
				 columns[definition->columns[c]].format,
				 means[p][c]);
			mp_table_cell(table, c + 2, "%s", text);
		}
	}
	mp_table_print(table, stdout);
	mp_table_free(table);
}

/*
 * scaling: exponent k of CPU time ~ chains^k per phase and algorithm,
 * least squares on the logarithms of the means
 */
static void print_complexity(const struct mp_point_spec *points,
			     const struct mp_result_row *rows, int count,
			     int point_count)
{
	static const char *phases[] = {"partitioning", "assignment",
				       "scheduling", "placement",
				       "communication", "total"};
	static double sums[6][MAX_POINTS], chains[MAX_POINTS];
	static int instances[MAX_POINTS];
	struct mp_table *table = mp_table_new(ALGORITHM_COUNT + 1);
	int i, p, phase, a;

	memset(sums, 0, sizeof(sums));
	memset(instances, 0, sizeof(instances));
	for (i = 0; i < count; i++) {
		const struct mp_result_row *row = &rows[i];
		const struct mp_phase_times *t = &row->times;

		if (row->error)
			continue;
		chains[row->point] = row->chain_count;
		instances[row->point]++;
		sums[0][row->point] += t->partitioning;
		sums[1][row->point] += t->assignment;
		sums[2][row->point] += t->scheduling;
		sums[3][row->point] += t->swapping + t->displacement +
				       t->repair;
		sums[4][row->point] += t->communication;
		sums[5][row->point] += t->total;
	}
	mp_table_header(table, 0, "phase", false);
	for (a = 0; a < ALGORITHM_COUNT; a++)
		mp_table_header(table, a + 1, mp_algorithm_name(algorithms[a]),
				true);
	for (phase = 0; phase < 6; phase++) {
		mp_table_row(table);
		mp_table_cell(table, 0, "%s", phases[phase]);
		for (a = 0; a < ALGORITHM_COUNT; a++) {
			double sx = 0, sy = 0, sxx = 0, sxy = 0, n = 0;

			for (p = 0; p < point_count; p++) {
				double x, y;

				if (points[p].params.algorithm != algorithms[a] ||
				    !instances[p] || sums[phase][p] <= 0)
					continue;
				x = log(chains[p]);
				y = log(sums[phase][p] / instances[p]);
				sx += x;
				sy += y;
				sxx += x * x;
				sxy += x * y;
				n++;
			}
			if (n < 2)
				mp_table_cell(table, a + 1, "-");
			else
				mp_table_cell(table, a + 1, "%.2f",
					      (n * sxy - sx * sy) /
					      (n * sxx - sx * sx));
		}
	}
	printf("\n  growth of the CPU time: exponent k of time ~ chains^k "
	       "(least squares)\n");
	mp_table_print(table, stdout);
	mp_table_free(table);
}

/* ---- running ----------------------------------------------------------- */

static void usage(FILE *out, const char *program)
{
	int i;

	fprintf(out, "usage: %s <experiment|all> [options]\n\n"
		"  --instances N   applications per point (default: per "
		"experiment)\n"
		"  --jobs N        threads running design points in parallel "
		"(default: all processors)\n"
		"  --seed N        experiment seed: instance i is generated from "
		"(seed, i) (default: $MP_SEED or 1)\n"
		"  --out FILE      CSV file (default: results/<name>.csv; not "
		"with all)\n"
		"  --quiet         no progress line\n\nexperiments:\n", program);
	for (i = 0; i < EXPERIMENT_COUNT; i++)
		fprintf(out, "  %-13s %s (%d instance%s)\n", experiments[i].name,
			experiments[i].description,
			experiments[i].default_instances,
			experiments[i].default_instances > 1 ? "s" : "");
	fprintf(out, "  %-13s every experiment in turn\n", "all");
}

/* options given on the command line, shared by every experiment run */
struct run_options {
	int instances;		/* < 1: the default of each experiment */
	int jobs;
	uint64_t seed;
	bool progress;
	const char *out_path;	/* NULL: results/<name>.csv */
};

/*
 * Run one experiment: design points, CSV file and summary table.
 * Return: the number of failed design points, or -1 on a fatal error.
 */
static int run_experiment(const struct experiment_def *definition,
			  const struct run_options *options)
{
	static struct mp_point_spec points[MAX_POINTS];
	static double means[MAX_POINTS][MAX_COLUMNS];
	struct mp_experiment experiment = {0};
	struct mp_result_row *rows;
	const char *out_path = options->out_path;
	char default_path[256];
	int i, count, failed = 0;
	FILE *out;

	experiment.name = definition->name;
	experiment.points = points;
	experiment.point_count = definition->make_points(points);
	experiment.instances = options->instances < 1 ?
			       definition->default_instances :
			       options->instances;
	experiment.jobs = options->jobs;
	experiment.seed = options->seed;
	experiment.progress = options->progress;
	if (!out_path) {
		mkdir("results", 0755);
		snprintf(default_path, sizeof(default_path), "results/%s.csv",
			 definition->name);
		out_path = default_path;
	}

	printf("\n  experiment %s: %s\n  %d points x %d instance%s = %d design "
	       "points, seed %llu\n\n", definition->name,
	       definition->description, experiment.point_count,
	       experiment.instances, experiment.instances > 1 ? "s" : "",
	       experiment.point_count * experiment.instances,
	       (unsigned long long)experiment.seed);
	fflush(stdout);
	if (mp_run_experiment(&experiment, &rows)) {
		fprintf(stderr, "out of memory\n");
		return -1;
	}
	count = experiment.point_count * experiment.instances;
	for (i = 0; i < count; i++)
		if (rows[i].error && !failed++)
			fprintf(stderr, "point %d instance %d failed: %s\n",
				rows[i].point, rows[i].instance,
				strerror(-rows[i].error));
	if (failed > 1)
		fprintf(stderr, "(%d failed design points in all)\n", failed);

	out = fopen(out_path, "w");
	if (!out) {
		perror(out_path);
		free(rows);
		return -1;
	}
	mp_write_csv(out, definition->name, rows, count);
	fclose(out);

	summarize(definition, rows, count, experiment.point_count, means);
	print_summary(definition, points, experiment.point_count, means);
	if (!strcmp(definition->name, "scaling"))
		print_complexity(points, rows, count, experiment.point_count);
	printf("  means over the instances (the feasible ones for the mapping "
	       "metrics); one row per design point in %s\n", out_path);
	free(rows);
	return failed;
}

int main(int argc, char **argv)
{
	static const struct option long_options[] = {
		{"instances", required_argument, NULL, 'n'},
		{"jobs", required_argument, NULL, 'j'},
		{"seed", required_argument, NULL, 's'},
		{"out", required_argument, NULL, 'o'},
		{"quiet", no_argument, NULL, 'q'},
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0},
	};
	const char *env_seed = getenv("MP_SEED");
	struct run_options options = {0};
	int option, i, failed = 0, result;
	bool all;

	options.seed = env_seed ? strtoull(env_seed, NULL, 10) : 1;
	options.progress = isatty(STDERR_FILENO);
	while ((option = getopt_long(argc, argv, "", long_options,
				     NULL)) != -1) {
		switch (option) {
		case 'n':
			options.instances = atoi(optarg);
			break;
		case 'j':
			options.jobs = atoi(optarg);
			break;
		case 's':
			options.seed = strtoull(optarg, NULL, 10);
			break;
		case 'o':
			options.out_path = optarg;
			break;
		case 'q':
			options.progress = false;
			break;
		case 'h':
			usage(stdout, argv[0]);
			return 0;
		default:
			usage(stderr, argv[0]);
			return 2;
		}
	}
	if (optind != argc - 1) {
		usage(stderr, argv[0]);
		return 2;
	}

	/* "all": every experiment in turn, each in results/<name>.csv */
	all = !strcmp(argv[optind], "all");
	if (all && options.out_path) {
		fprintf(stderr, "--out cannot be used with all\n");
		return 2;
	}
	for (i = 0; i < EXPERIMENT_COUNT; i++) {
		if (!all && strcmp(argv[optind], experiments[i].name))
			continue;
		result = run_experiment(&experiments[i], &options);
		if (result < 0)
			return 1;
		failed += result;
		if (!all)
			return failed ? 1 : 0;
	}
	if (!all) {
		fprintf(stderr, "unknown experiment '%s'\n\n", argv[optind]);
		usage(stderr, argv[0]);
		return 2;
	}
	printf("\n");
	return failed ? 1 : 0;
}
