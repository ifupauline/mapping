// SPDX-License-Identifier: Apache-2.0
/* Mapping metrics and final checks (see stats.h). */
#include <errno.h>
#include <string.h>

#include "mapping/let.h"
#include "mapping/sched.h"
#include "mapping/stats.h"

/*
 * on one core, the priorities keep the precedence order of each
 * activation pattern (tasks of one chain with the same period)
 */
static int verify_core_precedence(const struct mp_core *core)
{
	int i, j, k, l;

	for (i = 0; i < core->chains.count; i++) {
		const struct mp_chain *first = &core->chains.items[i];

		if (first->is_let)
			continue;
		for (j = 0; j < core->chains.count; j++) {
			const struct mp_chain *second = &core->chains.items[j];

			if (second->is_let || second->id != first->id)
				continue;
			/* a task before another of its pattern runs first */
			for (k = 0; k < first->tasks.count; k++)
				for (l = 0; l < second->tasks.count; l++) {
					const struct mp_task *a = &first->tasks.items[k];
					const struct mp_task *b = &second->tasks.items[l];

					if (a->period == b->period &&
					    a->id_in_chain < b->id_in_chain &&
					    a->priority >= b->priority)
						return -EINVAL;
				}
		}
	}
	return 0;
}

/* (C2): a task that must share a core with the next one does */
static int verify_core_colocation(const struct mp_core *core)
{
	int i, j, k, l;

	for (i = 0; i < core->chains.count; i++) {
		const struct mp_chain *fragment = &core->chains.items[i];

		if (fragment->is_let)
			continue;
		for (k = 0; k < fragment->tasks.count; k++) {
			const struct mp_task *a = &fragment->tasks.items[k];
			bool found = false;

			if (!a->colocate_next)
				continue;
			for (j = 0; j < core->chains.count && !found; j++) {
				const struct mp_chain *other =
					&core->chains.items[j];

				if (other->is_let || other->id != fragment->id)
					continue;
				for (l = 0; l < other->tasks.count; l++)
					if (other->tasks.items[l].id_in_chain ==
					    a->id_in_chain + 1)
						found = true;
			}
			if (!found)
				return -EINVAL;
		}
	}
	return 0;
}

int mp_verify_mapping(struct mp_core_array *cores)
{
	int i, err;

	for (i = 0; i < cores->count; i++) {
		struct mp_core *core = &cores->items[i];

		if (mp_core_update_load(core) > core->fill_limit)
			return -ERANGE;
		err = verify_core_precedence(core);
		if (!err)
			err = verify_core_colocation(core);
		if (err)
			return err;
	}
	return 0;
}

/* utilization of @fragment on the cheapest core of @controller, or -1 */
static int64_t cheapest_utilization(const struct mp_params *params,
				    const struct mp_chain *fragment,
				    int controller)
{
	unsigned types = mp_chain_types(fragment) &
			 mp_ecu_types(params, controller);
	int64_t best = -1;
	int type;

	if (!(mp_chain_allowed(fragment) & MP_CONTROLLER_BIT(controller)))
		return -1;
	for (type = 0; type < MP_CORE_TYPES; type++) {
		int64_t load;

		if (!(types & (1u << type)))
			continue;
		load = mp_chain_utilization_on(fragment,
					       mp_ecu_speed(params, controller,
							    type), type);
		if (best < 0 || load < best)
			best = load;
	}
	return best;
}

int mp_cores_lower_bound(const struct mp_params *params,
			 const struct mp_chain_array *fragments)
{
	int64_t bound[MP_MAX_ECUS] = {0}, total = 0, phi;
	int64_t cores = 0, by_capacity;
	int i, c;

	phi = params->fill_limit;
	if (phi <= 0)
		return 0;
	for (i = 0; i < fragments->count; i++) {
		const struct mp_chain *fragment = &fragments->items[i];
		int64_t cheapest = -1;

		if (fragment->is_let)
			continue;
		if (fragment->controller >= 0 &&
		    fragment->controller < MP_MAX_ECUS) {
			cheapest = cheapest_utilization(params, fragment,
							fragment->controller);
			if (cheapest > 0)
				bound[fragment->controller] += cheapest;
		} else {
			for (c = 0; c < mp_ecu_count(params); c++) {
				int64_t load = cheapest_utilization(params,
								    fragment, c);

				if (load >= 0 && (cheapest < 0 || load < cheapest))
					cheapest = load;
			}
		}
		if (cheapest > 0)
			total += cheapest;
	}
	for (c = 0; c < MP_MAX_ECUS; c++)
		cores += (bound[c] + phi - 1) / phi;
	by_capacity = (total + phi - 1) / phi;
	return (int)(by_capacity > cores ? by_capacity : cores);
}

void mp_compute_stats(const struct mp_ctx *ctx,
		      const struct mp_core_array *cores,
		      struct mp_stats *stats)
{
	int i, j, k;

	memset(stats, 0, sizeof(*stats));

	/* cores and tasks used */
	for (i = 0; i < cores->count; i++) {
		const struct mp_core *core = &cores->items[i];

		if (core->is_empty)
			continue;
		stats->used_cores++;
		if (mp_ecu_is_central(&ctx->params, core->controller))
			stats->central_cores++;
		else
			stats->other_cores++;
		if (core->type >= 0 && core->type < MP_CORE_TYPES)
			stats->type_cores[core->type]++;
		for (j = 0; j < core->chains.count; j++)
			for (k = 0; k < core->chains.items[j].tasks.count; k++)
				if (!core->chains.items[j].tasks.items[k].is_let)
					stats->task_count++;
	}

	/* schedulability rates and the gains of each placement method */
	stats->schedulable_cores = ctx->schedulable_cores;
	stats->unschedulable_cores = ctx->unschedulable_cores;
	stats->rate_scheduling = ctx->rate_after_scheduling * 100;
	stats->rate_swapping = ctx->rate_after_swapping * 100;
	stats->rate_displacement = ctx->rate_after_displacement * 100;
	stats->rate_final = ctx->rate_final * 100;
	stats->deadline_repairs = ctx->deadline_repairs;
	stats->emptied_cores = ctx->emptied_cores;
	stats->swapping_gain = stats->rate_swapping - stats->rate_scheduling;
	stats->displacement_gain = stats->rate_displacement -
				   stats->rate_swapping;
	stats->placement_gain = stats->swapping_gain + stats->displacement_gain;

	/* utilizations, summed in ppm then converted to cores */
	for (i = 0; i < cores->count; i++) {
		const struct mp_core *core = &cores->items[i];

		if (core->is_empty)
			continue;
		stats->application_utilization += core->load;
		stats->unused_utilization += (float)core->load_remaining;
		for (j = 0; j < core->chains.count; j++)
			if (core->chains.items[j].is_let && core->chains.count > 1)
				stats->let_utilization +=
					core->chains.items[j].utilization;
	}
	/* the LET load is not application load */
	stats->application_utilization -= stats->let_utilization;
	stats->let_utilization /= MP_UTILIZATION_UNIT;
	stats->application_utilization /= MP_UTILIZATION_UNIT;
	stats->unused_utilization /= MP_UTILIZATION_UNIT;
	stats->system_capacity = stats->used_cores *
		((float)ctx->params.fill_limit / (float)MP_UTILIZATION_UNIT);

	stats->fragmentation_rate = ((float)ctx->cuts / (float)ctx->max_cuts) *
				    100;

	/* sizing per controller and sensitivity */
	stats->scaling_factor = -1;
	for (i = 0; i < MP_MAX_ECUS; i++)
		stats->controllers[i].scaling_factor = -1;
	for (i = 0; i < cores->count; i++) {
		const struct mp_core *core = &cores->items[i];
		struct mp_controller_stats *controller;
		float factor;
		int let_load = 0;

		if (core->is_empty || core->controller < 0 ||
		    core->controller >= MP_MAX_ECUS)
			continue;
		controller = &stats->controllers[core->controller];
		for (j = 0; j < core->chains.count; j++)
			if (core->chains.items[j].is_let)
				let_load += core->chains.items[j].utilization;
		controller->used_cores++;
		for (j = 0; j < core->chains.count; j++)
			controller->memory += core->chains.items[j].memory;
		controller->application_utilization +=
			(float)(core->load - let_load) / MP_UTILIZATION_UNIT;
		controller->let_utilization += (float)let_load / MP_UTILIZATION_UNIT;

		/* the margin of every core in use: below 1 on a core that
		 * misses a deadline */
		factor = (float)mp_core_scaling_factor(core);
		if (factor < 0)
			continue;
		if (controller->scaling_factor < 0 ||
		    factor < controller->scaling_factor)
			controller->scaling_factor = factor;
		if (stats->scaling_factor < 0 || factor < stats->scaling_factor)
			stats->scaling_factor = factor;
	}
}
