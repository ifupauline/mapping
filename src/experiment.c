// SPDX-License-Identifier: Apache-2.0
/* Parallel experiment engine (see experiment.h). */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _OPENMP
#include <omp.h>
#endif

#include "mapping/experiment.h"
#include "mapping/generator.h"

void mp_point_spec_default(struct mp_point_spec *spec)
{
	memset(spec, 0, sizeof(*spec));
	spec->application = MP_APPLICATION_AUTOMOTIVE;
	mp_gen_params_default(&spec->generator);
	mp_params_default(&spec->params);
	mp_lifecycle_params_default(&spec->lifecycle);
}

uint64_t mp_instance_seed(uint64_t seed, int instance)
{
	/* splitmix64 finalizer: neighbouring instances get unrelated seeds */
	uint64_t z = seed * 0x9e3779b97f4a7c15ULL + (uint64_t)instance + 1;

	z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
	z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
	return z ^ (z >> 31);
}

static double wall_seconds(void)
{
	struct timespec now;

	clock_gettime(CLOCK_MONOTONIC, &now);
	return (double)now.tv_sec + (double)now.tv_nsec * 1e-9;
}

/* one line, rewritten in place: bar, percentage, count, elapsed, ETA */
static void show_progress(const char *name, int done, int total,
			  double elapsed)
{
	int width = 30, filled = (int)((double)done / total * width), i;
	double remaining = done ? elapsed / done * (total - done) : 0;

	fprintf(stderr, "\r  %-10s [", name);
	for (i = 0; i < width; i++)
		fputc(i < filled ? '#' : '.', stderr);
	fprintf(stderr, "] %3d%%  %d/%d  %.0fs  eta %.0fs ", 100 * done / total,
		done, total, elapsed, remaining);
	if (done == total)
		fputc('\n', stderr);
	fflush(stderr);
}

const char *mp_application_name(enum mp_application_kind kind)
{
	return kind == MP_APPLICATION_DETECTION ? "detection" : "automotive";
}

static int make_application(const struct mp_point_spec *spec,
			    struct mp_ctx *ctx,
			    struct mp_chain_array *application)
{
	if (spec->application == MP_APPLICATION_DETECTION)
		return mp_detection_chain_application(ctx, application);
	return mp_generate_application(ctx, &spec->generator, application);
}

static void fill_row(struct mp_result_row *row,
		     const struct mp_chain_array *application,
		     const struct mp_design_point *point)
{
	int i;

	row->chain_count = application->count;
	for (i = 0; i < application->count; i++)
		row->task_count += application->items[i].tasks.count;
	row->feasible = point->feasible;
	row->fragment_count = point->fragments.count;
	row->cuts = point->ctx.cuts;
	row->forced_cuts = point->ctx.forced_cuts;
	row->max_cuts = point->ctx.max_cuts;
	row->core_count = point->cores.count;
	row->architecture_cores = point->ctx.architecture_cores;
	row->stats = point->stats;
	row->communication = point->communication;
	row->times = point->times;
	row->sizing = point->sizing;
}

/* design point @index: its application, then the methodology */
static void run_one(const struct mp_experiment *experiment, int index,
		    struct mp_result_row *row)
{
	const struct mp_point_spec *spec;
	struct mp_chain_array application;
	struct mp_design_point point;
	struct mp_ctx ctx;

	memset(row, 0, sizeof(*row));
	row->instance = index / experiment->point_count;
	row->point = index % experiment->point_count;
	spec = &experiment->points[row->point];
	row->spec = *spec;
	row->application_seed = mp_instance_seed(experiment->seed,
						 row->instance);

	/* the same instance gives the same application at every point */
	mp_ctx_init(&ctx, row->application_seed);
	ctx.params = spec->params;
	row->error = make_application(spec, &ctx, &application);
	if (row->error)
		return;
	if (spec->lifecycle.updates > 0)
		row->error = mp_run_lifecycle(&ctx, &application,
					      &spec->generator, &spec->lifecycle,
					      row->application_seed, &point,
					      &row->lifecycle);
	else
		row->error = mp_run_design_point(&ctx, &application, NULL,
						 &point);
	if (!row->error)
		fill_row(row, &application, &point);
	mp_design_point_free(&point);
	mp_chain_array_free(&application);
}

int mp_run_experiment(const struct mp_experiment *experiment,
		      struct mp_result_row **rows)
{
	int total = experiment->point_count * experiment->instances;
	double start = wall_seconds(), last_shown = 0;
	struct mp_result_row *results;
	int done = 0, index;

	results = calloc((size_t)total ? (size_t)total : 1, sizeof(*results));
	if (!results)
		return -ENOMEM;
#ifdef _OPENMP
	if (experiment->jobs > 0)
		omp_set_num_threads(experiment->jobs);
#endif

	#pragma omp parallel for schedule(dynamic, 1)
	for (index = 0; index < total; index++) {
		run_one(experiment, index, &results[index]);

		#pragma omp critical(mp_progress)
		{
			double now = wall_seconds() - start;

			done++;
			/* at most 10 refreshes per second */
			if (experiment->progress &&
			    (done == total || now - last_shown > 0.1)) {
				show_progress(experiment->name, done, total, now);
				last_shown = now;
			}
		}
	}
	*rows = results;
	return 0;
}

void mp_write_csv(FILE *out, const char *experiment,
		  const struct mp_result_row *rows, int count)
{
	int i, c;

	fprintf(out, "experiment,point,label,application,instance,seed,"
		"sigma,algorithm,phi,pcu_speed,zcu_speed,let_wcet,"
		"wcet_margin,affinity,repair,link_mbps,preemption,"
		"deadline_pct,error,topology,backbone_mbps,classes,shaping,frer,"
		"pcu_memory,zcu_memory,separation,replica_pct,colocate_pct,"
		"zone_only_pct,pcu_little,zcu_lockstep,type_variation,"
		"lockstep_pct,target_scaling,memory_latency,access_density,"
		"chains,tasks,feasible,fragments,cuts,"
		"forced_cuts,max_cuts,fr,cores_total,cores_arch,cores_used,"
		"cores_lb,cores_pcu,cores_zcu,cores_big,cores_little,cores_mcu,"
		"cores_lockstep,sr_sched,sr_swap,sr_disp,sr_final,"
		"sr_chains,sr_on_time,deadline_misses,repairs,emptied,u_app,u_let,"
		"u_unused,hops_intra,hops_inter,net_overloaded,net_avg,net_max,"
		"port_max,backbone_max,ports_overloaded,frer_flows,latency_avg,latency_max,delay_avg,delay_max,react_avg,react_max,"
		"scaling,sized_phi,target_met,sizing_runs,t_sizing,cores_c0,cores_c1,cores_c2,cores_c3,cores_c4,cores_c5,"
		"u_app_c0,u_app_c1,u_app_c2,u_app_c3,u_app_c4,u_app_c5,"
		"scaling_c0,scaling_c1,scaling_c2,scaling_c3,scaling_c4,"
		"scaling_c5,memory_c0,memory_c1,memory_c2,memory_c3,memory_c4,"
		"memory_c5,port_out_c0,port_out_c1,port_out_c2,port_out_c3,"
		"port_out_c4,port_out_c5,port_in_c0,port_in_c1,port_in_c2,"
		"port_in_c3,port_in_c4,port_in_c5,updates,update_chains,"
		"update_phi,absorbed,platform_cores,");
	for (c = 0; c <= MP_LIFECYCLE_MAX_UPDATES; c++)
		fprintf(out, "life_scaling_%d,life_on_time_%d,life_load_%d,",
			c, c, c);
	fprintf(out, "t_partition,t_assign,"
		"t_sched,t_swap,t_disp,t_repair,t_comm,t_total\n");
	for (i = 0; i < count; i++) {
		const struct mp_result_row *row = &rows[i];
		const struct mp_stats *stats = &row->stats;
		const struct mp_communication_summary *comm = &row->communication;
		const struct mp_phase_times *times = &row->times;
		const struct mp_params *params = &row->spec.params;
		const struct mp_network_summary *net = &comm->network;

		/* parameters of the point */
		fprintf(out, "%s,%d,%s,%s,%d,%llu,%.3f,%s,%.3f,%d,%d,%d,%d,%d,"
			"%d,%d,%d,%d,%d,",
			experiment, row->point, row->spec.label,
			mp_application_name(row->spec.application),
			row->instance,
			(unsigned long long)row->application_seed,
			params->sigma / (double)MP_UTILIZATION_UNIT,
			mp_algorithm_name(params->algorithm),
			params->fill_limit / (double)MP_UTILIZATION_UNIT,
			params->pcu_speed, params->zcu_speed, params->let_wcet,
			params->wcet_margin, params->affinity,
			params->repair_deadlines, params->network.link_mbps,
			params->network.preemption,
			row->spec.generator.deadline_percent, row->error);
		fprintf(out, "%s,%d,%d,%s,%d,",
			mp_net_topology_name(params->network.topology),
			params->network.backbone_mbps ?
			params->network.backbone_mbps :
			params->network.link_mbps,
			params->network.class_count,
			mp_net_shaping_name(params->network.shaping),
			params->network.frer);
		fprintf(out, "%d,%d,%s,%d,%d,%d,", params->pcu_memory,
			params->zcu_memory,
			params->separation == MP_SEPARATE_CORES ? "cores" :
			"controllers", row->spec.generator.replica_percent,
			row->spec.generator.colocate_percent,
			row->spec.generator.zone_only_percent);
		fprintf(out, "%d,%d,%d,%d,%.3f,%d,%d,", params->pcu_little_cores,
			params->zcu_lockstep_cores,
			row->spec.generator.type_variation,
			row->spec.generator.lockstep_percent,
			params->target_scaling / 1000.0, params->memory_latency,
			params->access_density);
		/* application, partitioning, cores */
		fprintf(out, "%d,%d,%d,%d,%d,%d,%d,%.4f,%d,%d,%d,%d,%d,%d,",
			row->chain_count, row->task_count, row->feasible,
			row->fragment_count, row->cuts, row->forced_cuts,
			row->max_cuts, stats->fragmentation_rate,
			row->core_count, row->architecture_cores,
			stats->used_cores, stats->lower_bound_cores,
			stats->central_cores, stats->other_cores);
		fprintf(out, "%d,%d,%d,%d,", stats->type_cores[0],
			stats->type_cores[1], stats->type_cores[2],
			stats->type_cores[3]);
		/* schedulability, timing, loads */
		fprintf(out, "%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%d,%d,%d,"
			"%.4f,%.4f,%.4f,",
			stats->rate_scheduling, stats->rate_swapping,
			stats->rate_displacement, stats->rate_final,
			comm->schedulable_chain_rate, comm->on_time_chain_rate,
			comm->deadline_misses, stats->deadline_repairs,
			stats->emptied_cores, stats->application_utilization, stats->let_utilization,
			stats->unused_utilization);
		/* communication */
		fprintf(out, "%d,%d,%d,%.4f,%.4f,%.4f,%.4f,%d,%d,%.2f,%.2f,"
			"%.2f,%.2f,%.2f,%.2f,",
			comm->intra_ecu_hops, comm->inter_ecu_hops,
			net->overloaded_flows, net->average_delay,
			net->max_delay, net->max_port_utilization,
			net->max_backbone_utilization, net->overloaded_ports,
			net->frer_flows,
			comm->average_latency, comm->max_latency,
			comm->average_e2e_delay, comm->max_e2e_delay,
			comm->average_reaction_time, comm->max_reaction_time);
		/* sensitivity and sizing per controller */
		fprintf(out, "%.4f,%.3f,%d,%d,%.6f,", stats->scaling_factor,
			row->sizing.fill_limit / (double)MP_UTILIZATION_UNIT,
			row->sizing.target_met, row->sizing.runs,
			row->sizing.cpu_time);
		for (c = 0; c < MP_ZONAL_ECUS; c++)
			fprintf(out, "%d,", stats->controllers[c].used_cores);
		for (c = 0; c < MP_ZONAL_ECUS; c++)
			fprintf(out, "%.4f,",
				stats->controllers[c].application_utilization);
		for (c = 0; c < MP_ZONAL_ECUS; c++)
			fprintf(out, "%.4f,", stats->controllers[c].scaling_factor);
		for (c = 0; c < MP_ZONAL_ECUS; c++)
			fprintf(out, "%d,", stats->controllers[c].memory);
		for (c = 0; c < MP_ZONAL_ECUS; c++)
			fprintf(out, "%.4f,", net->egress_utilization[c]);
		for (c = 0; c < MP_ZONAL_ECUS; c++)
			fprintf(out, "%.4f,", net->ingress_utilization[c]);
		/* lifecycle: the SOP, then each absorbed update (empty) */
		fprintf(out, "%d,%d,%.3f,%d,%d,", row->spec.lifecycle.updates,
			row->spec.lifecycle.chains_per_update,
			row->spec.lifecycle.update_fill_limit /
			(double)MP_UTILIZATION_UNIT, row->lifecycle.absorbed,
			row->lifecycle.platform_cores);
		for (c = 0; c <= MP_LIFECYCLE_MAX_UPDATES; c++)
			if (row->spec.lifecycle.updates && row->feasible &&
			    c <= row->lifecycle.absorbed)
				fprintf(out, "%.4f,%.2f,%.4f,",
					row->lifecycle.scaling[c],
					row->lifecycle.on_time[c],
					row->lifecycle.load[c]);
			else
				fprintf(out, ",,,");
		fprintf(out, "%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n",
			times->partitioning, times->assignment,
			times->scheduling, times->swapping,
			times->displacement, times->repair,
			times->communication, times->total);
	}
}
