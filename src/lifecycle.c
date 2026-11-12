// SPDX-License-Identifier: Apache-2.0
/* Software updates on a fixed platform (see lifecycle.h). */
#include <errno.h>
#include <string.h>

#include "mapping/assign.h"
#include "mapping/let.h"
#include "mapping/lifecycle.h"
#include "mapping/partition.h"
#include "mapping/sched.h"
#include "mapping/stats.h"

void mp_lifecycle_params_default(struct mp_lifecycle_params *params)
{
	memset(params, 0, sizeof(*params));
	params->chains_per_update = 15;
	params->update_fill_limit = MP_FILL_LIMIT;
}

/* seed of update @update (splitmix64: unrelated to its neighbours) */
static uint64_t update_seed(uint64_t seed, int update)
{
	uint64_t z = seed * 0xbf58476d1ce4e5b9ULL + (uint64_t)update + 0x51;

	z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
	z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
	return z ^ (z >> 31);
}

/* the platform of the SOP: the cores that run tasks, renumbered */
static int platform_of(const struct mp_core_array *mapping,
		       struct mp_core_array *cores, int fill_limit)
{
	int i, err;

	memset(cores, 0, sizeof(*cores));
	for (i = 0; i < mapping->count; i++) {
		struct mp_core *core;

		if (!mp_core_period_gcd(&mapping->items[i]))
			continue;
		err = mp_core_array_push(cores, &mapping->items[i]);
		if (err)
			return err;
		core = &cores->items[cores->count - 1];
		core->fill_limit = fill_limit;
		core->load_remaining = fill_limit - core->load;
		err = mp_core_build_task_list(core, cores->count - 1);
		if (err)
			return err;
	}
	return 0;
}

/* the largest chain id, task id and replica group on @cores */
static void largest_ids(const struct mp_core_array *cores, int *chain,
			int *task, int *group)
{
	int i, j, k;

	*chain = *task = *group = 0;
	for (i = 0; i < cores->count; i++)
		for (j = 0; j < cores->items[i].chains.count; j++) {
			const struct mp_chain *c = &cores->items[i].chains.items[j];

			if (c->id > *chain)
				*chain = c->id;
			if (c->replica_group > *group)
				*group = c->replica_group;
			for (k = 0; k < c->tasks.count; k++)
				if (c->tasks.items[k].unique_id > *task)
					*task = c->tasks.items[k].unique_id;
		}
}

/* the task-chains of an update, numbered after those of @cores */
static void renumber(struct mp_chain_array *chains,
		     const struct mp_core_array *cores, struct mp_ctx *ctx)
{
	int chain, task, group, i, k;

	largest_ids(cores, &chain, &task, &group);
	if (ctx->next_chain_id > chain)
		chain = ctx->next_chain_id - 1;
	for (i = 0; i < chains->count; i++) {
		struct mp_chain *c = &chains->items[i];

		c->id += chain + 1;
		if (c->replica_group)
			c->replica_group += group;
		for (k = 0; k < c->tasks.count; k++) {
			c->tasks.items[k].chain_id = c->id;
			c->tasks.items[k].unique_id += task + 1;
		}
	}
	/* new LET chains, if any, after them */
	ctx->next_chain_id = chain + 1 + chains->count;
}

/* the metrics of the platform in step @step of @result */
static int record(struct mp_ctx *ctx, const struct mp_core_array *cores,
		  int chains, struct mp_lifecycle_result *result, int step)
{
	struct mp_communication_summary summary;
	struct mp_path_array paths;
	struct mp_stats stats;
	int err;

	memset(&paths, 0, sizeof(paths));
	err = mp_communication(cores, &ctx->params.network, &paths, &summary);
	mp_path_array_free(&paths);
	if (err)
		return err;
	mp_compute_stats(ctx, cores, &stats);
	result->chains[step] = chains;
	result->scaling[step] = stats.scaling_factor;
	result->load[step] = stats.application_utilization;
	result->on_time[step] = summary.on_time_chain_rate;
	result->misses[step] = summary.deadline_misses;
	return 0;
}

/* update @update on @cores: 1 if absorbed (then @cores holds it), else 0 */
static int one_update(struct mp_ctx *ctx, struct mp_core_array *cores,
		      const struct mp_gen_params *generator,
		      const struct mp_lifecycle_params *params, uint64_t seed,
		      int update)
{
	struct mp_chain_array chains;
	struct mp_core_array trial;
	struct mp_gen_params gen = *generator;
	struct mp_ctx gen_ctx;
	int err, i;

	gen.chain_count = params->chains_per_update;
	mp_ctx_init(&gen_ctx, update_seed(seed, update));
	gen_ctx.params = ctx->params;
	err = mp_generate_application(&gen_ctx, &gen, &chains);
	if (err)
		return err;
	renumber(&chains, cores, ctx);
	mp_prepare_chains(&ctx->params, &chains);
	err = mp_partition(ctx, &chains);
	memset(&trial, 0, sizeof(trial));
	if (!err)
		err = mp_core_array_copy(&trial, cores);
	if (!err)
		err = mp_assign(ctx, &chains, &trial);
	mp_chain_array_free(&chains);
	if (err || !ctx->feasible)
		goto out;
	for (i = 0; i < trial.count; i++) {
		err = mp_core_build_task_list(&trial.items[i], i);
		if (err)
			goto out;
	}
	err = mp_schedule(ctx, &trial);
	if (err)
		goto out;
	mp_count_schedulable_cores(ctx, &trial);
	if (ctx->unschedulable_cores)
		goto out;
	mp_core_array_free(cores);
	*cores = trial;
	return 1;
out:
	mp_core_array_free(&trial);
	return err;
}

int mp_run_lifecycle(const struct mp_ctx *base,
		     const struct mp_chain_array *application,
		     const struct mp_gen_params *generator,
		     const struct mp_lifecycle_params *params, uint64_t seed,
		     struct mp_design_point *sop,
		     struct mp_lifecycle_result *result)
{
	struct mp_core_array cores;
	struct mp_ctx ctx;
	int chains = application->count, updates, u, err;

	memset(result, 0, sizeof(*result));
	err = mp_run_design_point(base, application, NULL, sop);
	if (err || !sop->feasible)
		return err;

	/* the platform: fixed hardware, the limit of the updates */
	ctx = sop->ctx;
	ctx.params.fixed_hardware = true;
	ctx.params.fill_limit = params->update_fill_limit;
	ctx.params.target_scaling = 0;
	err = platform_of(&sop->cores, &cores, params->update_fill_limit);
	if (err)
		goto out;
	result->platform_cores = cores.count;
	err = record(&ctx, &cores, chains, result, 0);
	if (err)
		goto out;

	updates = params->updates < MP_LIFECYCLE_MAX_UPDATES ?
		  params->updates : MP_LIFECYCLE_MAX_UPDATES;
	for (u = 1; u <= updates; u++) {
		err = one_update(&ctx, &cores, generator, params, seed, u);
		if (err <= 0)
			break;
		chains += params->chains_per_update;
		result->absorbed = u;
		err = record(&ctx, &cores, chains, result, u);
		if (err)
			break;
	}
out:
	mp_core_array_free(&cores);
	return err < 0 ? err : 0;
}
