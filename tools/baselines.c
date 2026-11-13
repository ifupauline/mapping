// SPDX-License-Identifier: Apache-2.0
/*
 * baselines: the heuristic against simulated annealing and a genetic
 * algorithm (search.h) on the same generated applications.
 *
 * Usage: baselines <results.csv> <trace.csv> [options]
 *   --sizes LIST       task-chains per application (50,100,200,300)
 *   --seeds N          applications per size, seeds 1..N (10)
 *   --sa LIST          budgets of the annealing, moves
 *                      (20000,200000,1000000)
 *   --ga LIST          budgets of the genetic algorithm, generations
 *                      (5,25,100)
 *   --platform P       default, slow (zone cores at 25 %), constraints
 *                      (20 % replicas, 10 % zone-only, 8 MB per zone
 *                      controller) or domain: the platforms of the
 *                      optimality-gap study (default)
 *   --no-local-search  SA and GA without the deadline local search of
 *                      the heuristic on their best mapping
 *
 * results.csv: one row per application and method: the heuristic with
 * each allocation rule (consolidation and local search included, as in
 * the tool), SA and GA at each budget; cores used, the lower bound,
 * schedulable cores, task-chains on time, CPU time.
 * trace.csv: for SA and GA at their largest budget, every improvement of
 * the best mapping (CPU time, cores), for the quality-vs-time figure.
 * The applications follow the default generator, on the chosen platform;
 * runs are spread over the threads (OpenMP), each timed on its own
 * thread. SA and GA get the deadline local search of the heuristic on
 * their best mapping, so that the chains on time compare the searches,
 * not the local search.
 */
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mapping/generator.h"
#include "mapping/io.h"
#include "mapping/pipeline.h"
#include "mapping/search.h"

#define MAX_LIST 16

enum platform { PLATFORM_DEFAULT, PLATFORM_SLOW, PLATFORM_CONSTRAINTS,
		PLATFORM_DOMAIN };

static const char *const platform_names[] = {"default", "slow", "constraints",
					     "domain"};
static enum platform platform;
static bool local_search = true;

struct job {
	int chains, seed;
	int kind;		/* 0..3: a rule of the heuristic; 4: SA; 5: GA */
	long budget;
	/* results */
	int err;
	bool feasible;
	int cores, lower_bound;
	float schedulable, on_time;
	double cpu;
	struct mp_search_trace trace;
};

static int parse_list(const char *text, long *values)
{
	int n = 0;

	while (*text && n < MAX_LIST) {
		char *end;

		values[n++] = strtol(text, &end, 10);
		if (end == text)
			return -1;
		text = *end == ',' ? end + 1 : end;
	}
	return n;
}

static void run_job(struct job *job)
{
	static const enum mp_algorithm rules[] = {MP_BFDU, MP_WFDU, MP_FFDU,
						  MP_CADU};
	struct mp_chain_array application;
	struct mp_gen_params generator;
	struct mp_design_point point;
	struct mp_ctx ctx;

	mp_ctx_init(&ctx, (uint64_t)job->seed);
	mp_gen_params_default(&generator);
	generator.chain_count = job->chains;
	/* the platforms of exact/gap.py */
	switch (platform) {
	case PLATFORM_SLOW:
		ctx.params.zcu_speed = 25;
		break;
	case PLATFORM_CONSTRAINTS:
		generator.replica_percent = 20;
		generator.zone_only_percent = 10;
		ctx.params.zcu_memory = 8192;
		break;
	case PLATFORM_DOMAIN:
		job->err = mp_params_template(&ctx.params, "domain");
		if (job->err)
			return;
		break;
	default:
		break;
	}
	job->err = mp_generate_application(&ctx, &generator, &application);
	if (job->err)
		return;
	if (job->kind < 4) {
		ctx.params.algorithm = rules[job->kind];
		job->err = mp_run_design_point(&ctx, &application, NULL, &point);
	} else {
		struct mp_search_params sp;

		mp_search_params_default(&sp, job->kind == 4 ?
					 MP_SEARCH_ANNEALING : MP_SEARCH_GENETIC,
					 job->budget);
		sp.seed = (uint64_t)job->seed;
		sp.local_search = local_search;
		job->err = mp_search(&ctx, &application, &sp, &point,
				     &job->trace);
	}
	mp_chain_array_free(&application);
	if (job->err)
		return;
	job->feasible = point.feasible;
	job->cores = point.stats.used_cores;
	job->lower_bound = point.stats.lower_bound_cores;
	job->schedulable = point.stats.rate_final;
	job->on_time = point.communication.on_time_chain_rate;
	job->cpu = point.times.total;
	mp_design_point_free(&point);
}

static const char *method_name(const struct job *job)
{
	static const char *const names[] = {"BFDU", "WFDU", "FFDU", "CADU",
					    "SA", "GA"};

	return names[job->kind];
}

int main(int argc, char **argv)
{
	static const struct option options[] = {
		{"sizes", required_argument, NULL, 'n'},
		{"seeds", required_argument, NULL, 's'},
		{"sa", required_argument, NULL, 'a'},
		{"ga", required_argument, NULL, 'g'},
		{"platform", required_argument, NULL, 'p'},
		{"no-local-search", no_argument, NULL, 'l'},
		{NULL, 0, NULL, 0},
	};
	long sizes[MAX_LIST] = {50, 100, 200, 300}, sa[MAX_LIST] = {20000,
		200000, 1000000}, ga[MAX_LIST] = {5, 25, 100};
	int size_count = 4, sa_count = 3, ga_count = 3, seeds = 10;
	struct job *jobs;
	int option, count = 0, i, z, s, k, b;
	FILE *out, *trace;

	while ((option = getopt_long(argc, argv, "", options, NULL)) != -1) {
		switch (option) {
		case 'n':
			size_count = parse_list(optarg, sizes);
			break;
		case 's':
			seeds = atoi(optarg);
			break;
		case 'a':
			sa_count = parse_list(optarg, sa);
			break;
		case 'g':
			ga_count = parse_list(optarg, ga);
			break;
		case 'p':
			for (k = 0; k < 4 && strcmp(optarg, platform_names[k]); k++)
				;
			if (k == 4) {
				fprintf(stderr, "unknown platform %s\n", optarg);
				return 2;
			}
			platform = (enum platform)k;
			break;
		case 'l':
			local_search = false;
			break;
		default:
			return 2;
		}
	}
	if (argc - optind != 2 || size_count < 1 || sa_count < 1 ||
	    ga_count < 1 || seeds < 1) {
		fprintf(stderr, "usage: %s <results.csv> <trace.csv> [--sizes L] "
			"[--seeds N] [--sa L] [--ga L] [--platform P] "
			"[--no-local-search]\n", argv[0]);
		return 2;
	}
	jobs = calloc((size_t)size_count * seeds * (4 + sa_count + ga_count),
		      sizeof(*jobs));
	if (!jobs)
		return 1;
	for (z = 0; z < size_count; z++)
		for (s = 1; s <= seeds; s++) {
			for (k = 0; k < 4; k++) {
				jobs[count].chains = (int)sizes[z];
				jobs[count].seed = s;
				jobs[count++].kind = k;
			}
			for (b = 0; b < sa_count + ga_count; b++) {
				jobs[count].chains = (int)sizes[z];
				jobs[count].seed = s;
				jobs[count].kind = b < sa_count ? 4 : 5;
				jobs[count++].budget = b < sa_count ? sa[b] :
						       ga[b - sa_count];
			}
		}
#pragma omp parallel for schedule(dynamic, 1)
	for (i = 0; i < count; i++)
		run_job(&jobs[i]);

	out = fopen(argv[optind], "w");
	trace = fopen(argv[optind + 1], "w");
	if (!out || !trace) {
		perror("output");
		return 1;
	}
	fprintf(out, "platform,chains,seed,method,budget,feasible,cores,"
		"lower_bound,schedulable_percent,on_time_percent,cpu_s,"
		"evaluations\n");
	fprintf(trace, "chains,seed,method,budget,cpu_s,cores\n");
	for (i = 0; i < count; i++) {
		const struct job *job = &jobs[i];
		long largest = job->kind == 4 ? sa[sa_count - 1] :
			       ga[ga_count - 1];
		int t;

		if (job->err) {
			fprintf(stderr, "%d chains, seed %d, %s: %s\n", job->chains,
				job->seed, method_name(job), strerror(-job->err));
			return 1;
		}
		fprintf(out, "%s,%d,%d,%s,%ld,%d,%d,%d,%.2f,%.2f,%.6f,%ld\n",
			platform_names[platform], job->chains, job->seed,
			method_name(job), job->budget,
			job->feasible, job->feasible ? job->cores : -1,
			job->lower_bound, job->schedulable, job->on_time,
			job->cpu, job->trace.evaluations);
		if (job->kind >= 4 && job->budget == largest)
			for (t = 0; t < job->trace.count; t++)
				fprintf(trace, "%d,%d,%s,%ld,%.6f,%d\n",
					job->chains, job->seed, method_name(job),
					job->budget, job->trace.time[t],
					job->trace.cores[t]);
	}
	fclose(out);
	fclose(trace);
	free(jobs);
	return 0;
}
