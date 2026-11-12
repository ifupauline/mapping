// SPDX-License-Identifier: Apache-2.0
/*
 * export_instance: one instance of the assignment problem, for the exact
 * formulation (exact/exact.py), with the results of the four heuristics.
 *
 * Usage: export_instance <seed> <chains> <out.json> [options]
 *   --sigma S        partitioning limit, fraction of a core (0.4)
 *   --phi F          filling limit (0.8)
 *   --zcu-speed P    speed of the zone cores, % (50)
 *   --replicas P     % of replicated task-chains (0)
 *   --zone-only P    % of task-chains on the zone controllers only (0)
 *   --zcu-memory K   memory of each zone controller, KB (unlimited)
 *   --template T     platform: zonal (default), domain, hybrid or
 *                    distributed (not with --zcu-speed, --zcu-memory)
 *   --spare N        candidate cores per controller beyond the most any
 *                    heuristic uses or the architecture has (2)
 *   --verify FILE    instead of exporting, check an assignment: one
 *                    candidate core per fragment (whitespace separated),
 *                    placed with the constraints of the tool, then
 *                    scheduled; prints "valid <cores>" or the first
 *                    violation (exit status 1)
 *
 * The application is generated, partitioned once (the fragments do not
 * depend on the assignment rule) and mapped by BFDU, WFDU, FFDU and CADU,
 * with the consolidation and without it ("BFDU-" etc.).
 * The instance: the candidate cores (a kind of core: type, speed, LET
 * WCET), the fragments (allowed controllers and core types, memory,
 * replica group, and for every task its WCET on each kind, period and
 * position in its chain), the limits, and per heuristic: feasible, cores
 * used, every core schedulable, cores per controller, the assignment.
 * Shared-memory contention is not part of the exact model.
 */
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mapping/assign.h"
#include "mapping/generator.h"
#include "mapping/io.h"
#include "mapping/let.h"
#include "mapping/pipeline.h"
#include "mapping/sched.h"
#include "mapping/stats.h"

#define MAX_KINDS 8

static const enum mp_algorithm algorithms[] = {MP_BFDU, MP_WFDU, MP_FFDU,
					       MP_CADU};

struct kind {
	int type, speed, let_wcet;
};

static int kind_of(struct kind *kinds, int *count, const struct mp_core *core)
{
	int k;

	for (k = 0; k < *count; k++)
		if (kinds[k].type == core->type && kinds[k].speed == core->speed &&
		    kinds[k].let_wcet == core->let_wcet)
			return k;
	kinds[k].type = core->type;
	kinds[k].speed = core->speed;
	kinds[k].let_wcet = core->let_wcet;
	(*count)++;
	return k;
}

/* the fragment of @chain in @fragments, -1 if none */
static int fragment_index(const struct mp_chain_array *fragments,
			  const struct mp_chain *chain)
{
	int i;

	for (i = 0; i < fragments->count; i++)
		if (fragments->items[i].id == chain->id &&
		    fragments->items[i].fragment_index == chain->fragment_index)
			return i;
	return -1;
}

/* place @fragments on @cores as in @path, with the checks of the tool */
static int verify_assignment(struct mp_ctx *ctx, struct mp_core_array *cores,
			     const struct mp_chain_array *fragments,
			     const char *path)
{
	FILE *in = fopen(path, "r");
	int i, core, gcd, used = 0;

	if (!in) {
		perror(path);
		return 2;
	}
	for (i = 0; i < fragments->count; i++) {
		const struct mp_chain *f = &fragments->items[i];

		if (fscanf(in, "%d", &core) != 1 || core < 0 ||
		    core >= cores->count) {
			printf("bad assignment of fragment %d\n", i);
			fclose(in);
			return 1;
		}
		if (!mp_placement_allowed(ctx, cores, f, core, NULL)) {
			printf("fragment %d not allowed on core %d\n", i, core);
			fclose(in);
			return 1;
		}
		if (mp_core_load_with_chain(&cores->items[core], f, &gcd) >
		    cores->items[core].fill_limit) {
			printf("core %d above phi with fragment %d\n", core, i);
			fclose(in);
			return 1;
		}
		if (mp_core_add_chain(&cores->items[core], f, gcd))
			return 2;
	}
	fclose(in);
	if (mp_schedule(ctx, cores))
		return 2;
	for (i = 0; i < cores->count; i++) {
		if (!mp_core_period_gcd(&cores->items[i]))
			continue;
		used++;
		if (!cores->items[i].schedulable) {
			printf("core %d not schedulable\n", i);
			return 1;
		}
	}
	printf("valid %d\n", used);
	return 0;
}

int main(int argc, char **argv)
{
	static const struct option options[] = {
		{"sigma", required_argument, NULL, 's'},
		{"phi", required_argument, NULL, 'p'},
		{"zcu-speed", required_argument, NULL, 'z'},
		{"replicas", required_argument, NULL, 'r'},
		{"zone-only", required_argument, NULL, 'o'},
		{"zcu-memory", required_argument, NULL, 'm'},
		{"template", required_argument, NULL, 't'},
		{"spare", required_argument, NULL, 'x'},
		{"verify", required_argument, NULL, 'v'},
		{NULL, 0, NULL, 0},
	};
	struct mp_design_point points[8];
	struct mp_chain_array application;
	struct mp_core_array candidates = {0};
	struct mp_gen_params generator;
	struct kind kinds[MAX_KINDS];
	int most[MP_MAX_ECUS] = {0}, kind_count = 0, spare = 2;
	int option, a, c, i, j, k, n;
	const char *verify = NULL;
	struct mp_ctx ctx, build;
	uint64_t seed;
	FILE *out;

	mp_gen_params_default(&generator);
	mp_ctx_init(&ctx, 0);
	while ((option = getopt_long(argc, argv, "", options, NULL)) != -1) {
		switch (option) {
		case 's':
			ctx.params.sigma = (int)(atof(optarg) * MP_UTILIZATION_UNIT);
			break;
		case 'p':
			ctx.params.fill_limit = (int)(atof(optarg) *
						      MP_UTILIZATION_UNIT);
			break;
		case 'z':
			ctx.params.zcu_speed = atoi(optarg);
			break;
		case 'r':
			generator.replica_percent = atoi(optarg);
			break;
		case 'o':
			generator.zone_only_percent = atoi(optarg);
			break;
		case 'm':
			ctx.params.zcu_memory = atoi(optarg);
			break;
		case 't':
			if (mp_params_template(&ctx.params, optarg)) {
				fprintf(stderr, "unknown template %s\n", optarg);
				return 2;
			}
			break;
		case 'x':
			spare = atoi(optarg);
			break;
		case 'v':
			verify = optarg;
			break;
		default:
			return 2;
		}
	}
	if (argc - optind != 3) {
		fprintf(stderr, "usage: %s <seed> <chains> <out.json> [options]\n",
			argv[0]);
		return 2;
	}
	seed = (uint64_t)atoll(argv[optind]);
	generator.chain_count = atoi(argv[optind + 1]);
	mp_rng_seed(&ctx.rng, seed);
	if (mp_generate_application(&ctx, &generator, &application)) {
		fprintf(stderr, "cannot generate the application\n");
		return 1;
	}
	for (a = 0; a < 8; a++) {
		ctx.params.algorithm = algorithms[a % 4];
		ctx.params.consolidate = a < 4;
		if (mp_run_design_point(&ctx, &application, NULL, &points[a]))
			return 1;
		if (points[a].fragments.count != points[0].fragments.count) {
			fprintf(stderr, "the fragments depend on the rule\n");
			return 1;
		}
		for (i = 0; i < points[a].cores.count; i++) {
			int count = 0;

			c = points[a].cores.items[i].controller;
			for (j = 0; j < points[a].cores.count; j++)
				count += points[a].cores.items[j].controller == c;
			if (count > most[c])
				most[c] = count;
		}
	}

	/* candidate cores: the architecture, then cores of the default type */
	ctx.params.consolidate = true;
	build = ctx;
	if (mp_architecture_init(&build, &candidates))
		return 1;
	for (c = 0; c < mp_ecu_count(&ctx.params); c++) {
		int have = 0, max = mp_ecu_max_cores(&ctx.params, c);

		for (i = 0; i < candidates.count; i++)
			have += candidates.items[i].controller == c;
		for (n = have; n < (most[c] > have ? most[c] : have) + spare &&
		     (!max || n < max); n++)
			if (mp_add_core(&build, &candidates, c))
				return 1;
	}

	if (verify) {
		int status = verify_assignment(&build, &candidates,
					       &points[0].fragments, verify);

		for (a = 0; a < 8; a++)
			mp_design_point_free(&points[a]);
		mp_core_array_free(&candidates);
		mp_chain_array_free(&application);
		return status;
	}
	out = fopen(argv[optind + 2], "w");
	if (!out) {
		perror(argv[optind + 2]);
		return 1;
	}
	fprintf(out, "{\n\"seed\": %llu, \"chains\": %d,\n",
		(unsigned long long)seed, application.count);
	fprintf(out, "\"phi\": %d, \"sigma\": %d, \"separation\": \"%s\",\n",
		ctx.params.fill_limit, ctx.params.sigma,
		ctx.params.separation == MP_SEPARATE_CORES ? "cores" :
		"controllers");
	/* the ECUs: memory, central computers, the value of a free fragment */
	fprintf(out, "\"any\": %d, \"ecu_memory\": [", MP_CONTROLLER_ANY);
	for (c = 0; c < mp_ecu_count(&ctx.params); c++)
		fprintf(out, "%s%d", c ? ", " : "",
			mp_ecu_memory(&ctx.params, c));
	fprintf(out, "], \"central\": [");
	for (c = 0, n = 0; c < mp_ecu_count(&ctx.params); c++)
		if (mp_ecu_is_central(&ctx.params, c))
			fprintf(out, "%s%d", n++ ? ", " : "", c);
	fprintf(out, "],\n");
	fprintf(out, "\"lower_bound\": %d,\n",
		mp_cores_lower_bound(&ctx.params, &points[0].fragments));

	/* kinds and cores */
	for (i = 0; i < candidates.count; i++)
		kind_of(kinds, &kind_count, &candidates.items[i]);
	fprintf(out, "\"kinds\": [");
	for (k = 0; k < kind_count; k++)
		fprintf(out, "%s{\"type\": %d, \"speed\": %d, \"let_wcet\": %d}",
			k ? ", " : "", kinds[k].type, kinds[k].speed,
			kinds[k].let_wcet);
	fprintf(out, "],\n\"cores\": [");
	for (i = 0; i < candidates.count; i++)
		fprintf(out, "%s{\"controller\": %d, \"kind\": %d}", i ? ", " : "",
			candidates.items[i].controller,
			kind_of(kinds, &kind_count, &candidates.items[i]));
	fprintf(out, "],\n\"fragments\": [\n");

	/* fragments, in the order of the partitioning (decreasing load) */
	for (i = 0; i < points[0].fragments.count; i++) {
		const struct mp_chain *f = &points[0].fragments.items[i];

		fprintf(out, "%s{\"chain\": %d, \"index\": %d, \"allowed\": %u, "
			"\"types\": %u, \"memory\": %d, \"controller\": %d, "
			"\"replica\": %d, \"tasks\": [", i ? ",\n" : "", f->id,
			f->fragment_index, mp_chain_allowed(f),
			mp_chain_types(f), f->memory, f->controller,
			f->replica_group);
		for (j = 0; j < f->tasks.count; j++) {
			const struct mp_task *t = &f->tasks.items[j];

			fprintf(out, "%s{\"period\": %d, \"id\": %d, \"wcet\": [",
				j ? ", " : "", t->period, t->id_in_chain);
			for (k = 0; k < kind_count; k++)
				fprintf(out, "%s%d", k ? ", " : "",
					mp_task_wcet_on(t, kinds[k].speed,
							kinds[k].type));
			fprintf(out, "]}");
		}
		fprintf(out, "]}");
	}
	fprintf(out, "\n],\n\"heuristics\": {\n");

	/* the heuristics: fragment -> controller and its rank there */
	for (a = 0; a < 8; a++) {
		const struct mp_design_point *p = &points[a];
		int *core_of = calloc((size_t)p->fragments.count + 1,
				      sizeof(*core_of));

		if (!core_of)
			return 1;
		for (i = 0; i < p->cores.count; i++)
			for (j = 0; j < p->cores.items[i].chains.count; j++) {
				const struct mp_chain *chain =
					&p->cores.items[i].chains.items[j];
				int index;

				if (chain->is_let)
					continue;
				index = fragment_index(&points[0].fragments, chain);
				if (index >= 0)
					core_of[index] = i;
			}
		fprintf(out, "%s\"%s%s\": {\"feasible\": %s, \"cores\": %d, "
			"\"schedulable\": %s, \"core_controller\": [",
			a ? ",\n" : "", mp_algorithm_name(algorithms[a % 4]),
			a < 4 ? "" : "-",
			p->feasible ? "true" : "false", p->stats.used_cores,
			p->feasible && !p->stats.unschedulable_cores ? "true" :
			"false");
		for (i = 0; i < p->cores.count; i++)
			fprintf(out, "%s%d", i ? ", " : "",
				p->cores.items[i].controller);
		fprintf(out, "], \"assignment\": [");
		for (i = 0; i < p->fragments.count; i++)
			fprintf(out, "%s%d", i ? ", " : "",
				p->feasible ? core_of[i] : -1);
		fprintf(out, "]}");
		free(core_of);
	}
	fprintf(out, "\n}\n}\n");
	fclose(out);

	for (a = 0; a < 8; a++)
		mp_design_point_free(&points[a]);
	mp_core_array_free(&candidates);
	mp_chain_array_free(&application);
	return 0;
}
