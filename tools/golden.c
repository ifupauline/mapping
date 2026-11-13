// SPDX-License-Identifier: Apache-2.0
/*
 * golden: write reference results of the whole pipeline, one file per
 * application, for the regression test (tests/run_golden.sh compares a new
 * run with tests/golden byte for byte).
 *
 * Usage: golden <directory>
 *
 * Applications: generated ones of 60 chains (seeds 1 to 4), two of 200
 * chains (seeds 5 and 32) on cores filled up to 100 % and zone
 * controllers at a quarter of the PCU speed, so that swapping and
 * displacement have work to do, and the detection chain.
 * Each is mapped with sigma 0.03, 0.1 and 0.4 and the four assignment
 * algorithms. Every value that a change of the
 * methodology could move is written: fragments, cores and their tasks
 * (priority, response time), statistics, paths and network bounds.
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mapping/generator.h"
#include "mapping/pipeline.h"

/* the generated applications: seed, chains, filled cores */
static const struct {
	int seed;
	int chains;
	bool full;
} applications[] = {
	{1, 60, false}, {2, 60, false}, {3, 60, false}, {4, 60, false},
	{5, 200, true}, {32, 200, true},
};

#define APPLICATIONS ((int)(sizeof(applications) / sizeof(applications[0])))

static void dump_cores(FILE *out, const struct mp_core_array *cores)
{
	int i, j, k;

	for (i = 0; i < cores->count; i++) {
		const struct mp_core *core = &cores->items[i];

		if (core->is_empty)
			continue;
		fprintf(out, "  core %d controller %d speed %d load %d sched %d\n",
			core->id, core->controller, core->speed, core->load,
			core->schedulable);
		for (j = 0; j < core->chains.count; j++) {
			const struct mp_chain *chain = &core->chains.items[j];

			fprintf(out, "    chain %d.%d u %d let %d tasks", chain->id,
				chain->fragment_index, chain->utilization,
				chain->is_let);
			for (k = 0; k < chain->tasks.count; k++) {
				const struct mp_task *task = &chain->tasks.items[k];

				fprintf(out, " [%d c %d t %d p %d r %d]",
					task->id_in_chain, task->wcet,
					task->period, task->priority,
					task->response_time);
			}
			fprintf(out, "\n");
		}
	}
}

static void dump_point(FILE *out, const struct mp_design_point *point)
{
	const struct mp_stats *stats = &point->stats;
	const struct mp_communication_summary *comm = &point->communication;
	int i, j;

	fprintf(out, "fragments %d cuts %d forced %d max %d\n",
		point->fragments.count, point->ctx.cuts, point->ctx.forced_cuts,
		point->ctx.max_cuts);
	if (!point->feasible) {
		fprintf(out, "unfeasible\n");
		return;
	}
	dump_cores(out, &point->cores);
	fprintf(out, "stats cores %d pcu %d zcu %d sr %.3f %.3f %.3f %.3f "
		"repairs %d scaling %.3f u_app %.4f u_let %.4f\n",
		stats->used_cores, stats->central_cores, stats->other_cores,
		stats->rate_scheduling, stats->rate_swapping,
		stats->rate_displacement, stats->rate_final,
		stats->deadline_repairs, stats->scaling_factor,
		stats->application_utilization, stats->let_utilization);
	fprintf(out, "comm intra %d inter %d on_time %d misses %d net_max %.3f "
		"port_max %.3f\n", comm->intra_ecu_hops, comm->inter_ecu_hops,
		comm->on_time_chains, comm->deadline_misses,
		comm->network.max_delay, comm->network.max_port_utilization);
	for (i = 0; i < point->paths.count; i++) {
		const struct mp_chain_path *path = &point->paths.items[i];

		fprintf(out, "  path %d latency %d delay %d reaction %d on_time %d",
			path->chain_id, path->latency, path->e2e_delay,
			path->reaction_time, path->on_time);
		for (j = 0; j < path->segment_count; j++)
			fprintf(out, " [%d %d-%d]", path->segments[j].core_id,
				path->segments[j].first_fragment,
				path->segments[j].last_fragment);
		for (j = 0; j < path->hop_count; j++)
			fprintf(out, " (%.3f)", path->hops[j].flow.total_delay);
		fprintf(out, "\n");
	}
}

/* every design point of one application into @path */
static int dump_application(const char *path, struct mp_ctx *ctx,
			    const struct mp_chain_array *application)
{
	static const enum mp_algorithm algorithms[] = {MP_BFDU, MP_WFDU,
						       MP_FFDU, MP_CADU};
	static const int sigmas[] = {30000, 100000, 400000};
	FILE *out = fopen(path, "w");
	int s, a, err = 0;

	if (!out)
		return -errno;
	for (s = 0; s < 3 && !err; s++)
		for (a = 0; a < 4 && !err; a++) {
			struct mp_design_point point;

			ctx->params.sigma = sigmas[s];
			ctx->params.algorithm = algorithms[a];
			fprintf(out, "design point sigma %d algorithm %s\n",
				sigmas[s], mp_algorithm_name(algorithms[a]));
			err = mp_run_design_point(ctx, application, NULL, &point);
			if (!err)
				dump_point(out, &point);
			mp_design_point_free(&point);
		}
	fclose(out);
	return err;
}

int main(int argc, char **argv)
{
	struct mp_chain_array application;
	struct mp_gen_params generator;
	char path[512];
	struct mp_ctx ctx;
	int seed, err, i;

	if (argc != 2) {
		fprintf(stderr, "usage: %s <directory>\n", argv[0]);
		return 2;
	}
	mp_gen_params_default(&generator);
	for (i = 0; i < APPLICATIONS; i++) {
		seed = applications[i].seed;
		mp_ctx_init(&ctx, (uint64_t)seed);
		generator.chain_count = applications[i].chains;
		if (applications[i].full) {
			ctx.params.fill_limit = MP_UTILIZATION_UNIT;
			ctx.params.zcu_speed = 25;
		}
		err = mp_generate_application(&ctx, &generator, &application);
		snprintf(path, sizeof(path), "%s/seed_%d.txt", argv[1], seed);
		if (!err)
			err = dump_application(path, &ctx, &application);
		mp_chain_array_free(&application);
		if (err) {
			fprintf(stderr, "seed %d: %s\n", seed, strerror(-err));
			return 1;
		}
	}
	mp_ctx_init(&ctx, 1);
	err = mp_detection_chain_application(&ctx, &application);
	snprintf(path, sizeof(path), "%s/detection_chain.txt", argv[1]);
	if (!err)
		err = dump_application(path, &ctx, &application);
	mp_chain_array_free(&application);
	if (err) {
		fprintf(stderr, "detection chain: %s\n", strerror(-err));
		return 1;
	}
	return 0;
}
