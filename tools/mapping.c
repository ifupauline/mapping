// SPDX-License-Identifier: Apache-2.0
/*
 * mapping: run the method on one application and one platform, and print
 * the results; with --cores, the mapping itself (cores on ECUs,
 * task-chains on cores); with --out, the results as CSV and JSON files.
 *
 * Usage: mapping [APPLICATION.json [CONFIGURATION.json]] [options]
 *	  (mapping --help lists the options)
 */
#define _GNU_SOURCE
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "mapping/assign.h"
#include "mapping/generator.h"
#include "mapping/io.h"
#include "mapping/let.h"
#include "mapping/network.h"
#include "mapping/pipeline.h"
#include "mapping/sched.h"
#include "mapping/table.h"

#define ALGORITHMS 4

/* a utilization in ppm as a percentage */
static double percent(int ppm)
{
	return 100.0 * ppm / MP_UTILIZATION_UNIT;
}

/* ---- results of the allocation rules, side by side ---------------------- */

struct run {
	enum mp_algorithm algorithm;
	struct mp_design_point point;
};

/* one row: a label, then one cell per rule ("-" if unfeasible) */
#define RESULT_ROW(table, label, format, expression)			\
	do {								\
		int a_;							\
		mp_table_row(table);					\
		mp_table_cell(table, 0, "%s", label);			\
		for (a_ = 0; a_ < run_count; a_++) {			\
			const struct mp_design_point *p_ = &runs[a_].point; \
			if (p_->feasible)				\
				mp_table_cell(table, a_ + 1, format, expression); \
			else						\
				mp_table_cell(table, a_ + 1, "-");	\
		}							\
	} while (0)

/*
 * why @fragment found no core: the memory of the ECUs it may run on, the
 * fixed hardware, their largest number of cores, or its core types
 */
static void explain_infeasible(const struct mp_design_point *p,
			       const struct mp_chain *fragment)
{
	const struct mp_params *params = &p->ctx.params;
	unsigned allowed = mp_chain_allowed(fragment);
	int c, best_left = -1, ecus = 0, full = 0, room = 0, replicas = 0;

	printf(" (%.1f %% of a reference core, %d KB, %s%s)",
	       percent(fragment->utilization), fragment->memory,
	       fragment->controller == MP_CONTROLLER_ANY ? "free" :
	       "bound to ", fragment->controller == MP_CONTROLLER_ANY ? "" :
	       mp_ecu_name(params, fragment->controller));
	for (c = 0; c < mp_ecu_count(params); c++) {
		int capacity = mp_ecu_memory(params, c), max, have = 0, i;

		if (!(allowed & MP_CONTROLLER_BIT(c)) ||
		    !(mp_chain_types(fragment) & mp_ecu_types(params, c)))
			continue;
		ecus++;
		if (capacity) {
			int left = capacity - mp_controller_memory(&p->cores, c);

			/* the memory still reserved for the fragments bound
			 * to this ECU and not placed yet */
			if (fragment->controller != c)
				for (i = 0; i < p->fragments.count; i++)
					if (p->fragments.items[i].controller == c &&
					    !p->fragments.items[i].is_assigned)
						left -= p->fragments.items[i].memory;

			if (left > best_left)
				best_left = left;
			if (left < fragment->memory)
				continue;
		} else {
			best_left = 1 << 30;
		}
		room++;
		/* a replica of its group already on this ECU */
		if (fragment->replica_group > 0 &&
		    params->separation != MP_SEPARATE_CORES) {
			bool there = false;
			int j;

			for (i = 0; i < p->cores.count && !there; i++) {
				const struct mp_core *core = &p->cores.items[i];

				if (core->controller != c)
					continue;
				for (j = 0; j < core->chains.count; j++)
					if (!core->chains.items[j].is_let &&
					    core->chains.items[j].replica_group ==
					    fragment->replica_group &&
					    core->chains.items[j].id !=
					    fragment->id)
						there = true;
			}
			if (there) {
				replicas++;
				continue;
			}
		}
		max = mp_ecu_max_cores(params, c);
		for (i = 0; i < p->cores.count; i++)
			have += p->cores.items[i].controller == c;
		full += max && have >= max;
	}
	if (!ecus)
		printf(": no ECU it may run on has a core of a type it allows\n");
	else if (best_left >= 0 && best_left < fragment->memory)
		printf(": not enough memory, at most %d KB left on the ECUs it "
		       "may run on (memory reserved for the tasks bound to an "
		       "ECU counted)\n", best_left < 0 ? 0 : best_left);
	else if (room && replicas == room)
		printf(": a replica of its task-chain already runs on every ECU "
		       "with memory left that it may run on\n");
	else if (params->fixed_hardware)
		printf(": no core can take it and the hardware is fixed\n");
	else if (full)
		printf(": no core can take it and the ECUs with memory left "
		       "have their largest number of cores\n");
	else
		printf(": it does not fit even an empty core of a type it "
		       "allows\n");
}

static void print_results(const struct run *runs, int run_count)
{
	struct mp_table *table = mp_table_new(run_count + 1);
	int a;

	mp_table_header(table, 0, "metric", false);
	for (a = 0; a < run_count; a++)
		mp_table_header(table, a + 1,
				mp_algorithm_name(runs[a].algorithm), true);

	mp_table_row(table);
	mp_table_cell(table, 0, "feasible");
	for (a = 0; a < run_count; a++)
		mp_table_cell(table, a + 1, "%s",
			      runs[a].point.feasible ? "yes" : "no");
	RESULT_ROW(table, "sub-chains", "%d", p_->fragments.count);
	RESULT_ROW(table, "fragmentation rate FR (%)", "%.1f",
		   p_->stats.fragmentation_rate);
	RESULT_ROW(table, "cuts forced by the ECUs", "%d", p_->ctx.forced_cuts);
	mp_table_rule(table);
	RESULT_ROW(table, "cores used M", "%d", p_->stats.used_cores);
	RESULT_ROW(table, "  on central computers", "%d",
		   p_->stats.central_cores);
	RESULT_ROW(table, "  on the other ECUs", "%d", p_->stats.other_cores);
	RESULT_ROW(table, "  lower bound on M", "%d",
		   p_->stats.lower_bound_cores);
	RESULT_ROW(table, "cores added to the architecture", "%d",
		   p_->cores.count - p_->ctx.architecture_cores);
	RESULT_ROW(table, "cores emptied by consolidation", "%d",
		   p_->stats.emptied_cores);
	mp_table_rule(table);
	RESULT_ROW(table, "schedulable cores SR (%)", "%.1f",
		   p_->stats.rate_final);
	RESULT_ROW(table, "  after scheduling", "%.1f",
		   p_->stats.rate_scheduling);
	RESULT_ROW(table, "  after swap moves", "%.1f", p_->stats.rate_swapping);
	RESULT_ROW(table, "  after relocation moves", "%.1f",
		   p_->stats.rate_displacement);
	RESULT_ROW(table, "critical scaling factor", "%.3f",
		   p_->stats.scaling_factor);
	if (runs[0].point.ctx.params.target_scaling) {
		RESULT_ROW(table, "  cap found by the sizing", "%.3f",
			   p_->sizing.fill_limit /
			   (double)MP_UTILIZATION_UNIT);
		RESULT_ROW(table, "  target reached", "%s",
			   p_->sizing.target_met ? "yes" : "no");
		RESULT_ROW(table, "  design points run", "%d", p_->sizing.runs);
	}
	mp_table_rule(table);
	RESULT_ROW(table, "application load (cores)", "%.2f",
		   p_->stats.application_utilization);
	RESULT_ROW(table, "LET load (cores)", "%.2f",
		   p_->stats.let_utilization);
	RESULT_ROW(table, "room left below the cap (cores)", "%.2f",
		   p_->stats.unused_utilization);
	mp_table_rule(table);
	RESULT_ROW(table, "task-chains schedulable (%)", "%.1f",
		   p_->communication.schedulable_chain_rate);
	RESULT_ROW(table, "task-chains meeting deadline (%)", "%.1f",
		   p_->communication.on_time_chain_rate);
	RESULT_ROW(table, "  deadline misses", "%d",
		   p_->communication.deadline_misses);
	RESULT_ROW(table, "  local search moves", "%d", p_->stats.deadline_repairs);
	RESULT_ROW(table, "communications inside an ECU", "%d",
		   p_->communication.intra_ecu_hops);
	RESULT_ROW(table, "inter-ECU communications", "%d",
		   p_->communication.inter_ecu_hops);
	RESULT_ROW(table, "  replicated with FRER", "%d",
		   p_->communication.network.frer_flows);
	RESULT_ROW(table, "network delay bound, max (us)", "%.1f",
		   p_->communication.network.max_delay);
	RESULT_ROW(table, "busiest port load (%)", "%.2f",
		   p_->communication.network.max_port_utilization);
	RESULT_ROW(table, "end-to-end response time, max (ms)", "%.2f",
		   p_->communication.max_latency / 1e3);
	RESULT_ROW(table, "max reaction time, max (ms)", "%.2f",
		   p_->communication.max_reaction_time / 1e3);
	mp_table_rule(table);
	RESULT_ROW(table, "CPU time (ms)", "%.1f", p_->times.total * 1e3);
	if (runs[0].point.ctx.params.target_scaling)
		RESULT_ROW(table, "  with the sizing (ms)", "%.1f",
			   p_->sizing.cpu_time * 1e3);

	mp_table_print(table, stdout);
	mp_table_free(table);
	for (a = 0; a < run_count; a++) {
		const struct mp_design_point *p = &runs[a].point;
		const struct mp_chain *fragment = NULL;
		int i;

		if (p->feasible)
			continue;
		for (i = 0; i < p->fragments.count; i++)
			if (p->fragments.items[i].id == p->ctx.unplaced_chain &&
			    p->fragments.items[i].fragment_index ==
			    p->ctx.unplaced_fragment)
				fragment = &p->fragments.items[i];
		printf("  %s: infeasible, sub-chain %d.%d", mp_algorithm_name(
		       runs[a].algorithm), p->ctx.unplaced_chain,
		       p->ctx.unplaced_fragment);
		if (fragment)
			explain_infeasible(p, fragment);
		else
			printf(" fits on no core\n");
	}
}

/* cores, load and margin of every ECU, for every rule */
static void print_sizing(const struct mp_params *params, const struct run *runs,
			 int run_count)
{
	struct mp_table *table = mp_table_new(1 + 3 * run_count);
	char header[64];
	int a, c;

	mp_table_header(table, 0, "ECU", false);
	for (a = 0; a < run_count; a++) {
		const char *name = mp_algorithm_name(runs[a].algorithm);

		snprintf(header, sizeof(header), "%s\ncores", name);
		mp_table_header(table, 1 + 3 * a, header, true);
		snprintf(header, sizeof(header), "%s\nload", name);
		mp_table_header(table, 2 + 3 * a, header, true);
		snprintf(header, sizeof(header), "%s\nscaling", name);
		mp_table_header(table, 3 + 3 * a, header, true);
	}
	for (c = 0; c < mp_ecu_count(params); c++) {
		mp_table_row(table);
		mp_table_cell(table, 0, "%s", mp_ecu_name(params, c));
		for (a = 0; a < run_count; a++) {
			const struct mp_controller_stats *ecu =
				&runs[a].point.stats.controllers[c];

			if (!runs[a].point.feasible)
				continue;
			mp_table_cell(table, 1 + 3 * a, "%d", ecu->used_cores);
			mp_table_cell(table, 2 + 3 * a, "%.2f",
				      ecu->application_utilization +
				      ecu->let_utilization);
			if (ecu->scaling_factor < 0)
				mp_table_cell(table, 3 + 3 * a, "-");
			else
				mp_table_cell(table, 3 + 3 * a, "%.3f",
					      ecu->scaling_factor);
		}
	}
	printf("\n  cores used per ECU, their load (in cores, LET included) "
	       "and critical scaling factor\n");
	mp_table_print(table, stdout);
	mp_table_free(table);
}

/* ---- the mapping itself (--cores) ------------------------------------- */

static const char *type_label(int type)
{
	static const char *names[] = {"big", "little", "MCU", "lock-step"};

	return type >= 0 && type < MP_CORE_TYPES ? names[type] : "?";
}

static const struct mp_core *find_core(const struct mp_core_array *cores,
				       int core_id)
{
	int i;

	for (i = 0; i < cores->count; i++)
		if (cores->items[i].id == core_id)
			return &cores->items[i];
	return NULL;
}

/* "c12@ZCU3": a core and its ECU */
static void core_label(char *text, size_t size, const struct mp_params *params,
		       const struct mp_core_array *cores, int core_id)
{
	const struct mp_core *core = find_core(cores, core_id);

	if (core)
		snprintf(text, size, "c%d@%s", core_id,
			 mp_ecu_name(params, core->controller));
	else
		snprintf(text, size, "c%d", core_id);
}

/* the LET load and the application tasks of @core */
static void core_contents(const struct mp_core *core, int *let_load,
			  int *tasks)
{
	int j;

	*let_load = 0;
	*tasks = 0;
	for (j = 0; j < core->chains.count; j++) {
		const struct mp_chain *chain = &core->chains.items[j];

		if (chain->is_let)
			*let_load = chain->utilization;
		else
			*tasks += chain->tasks.count;
	}
}

/* the cores in id order: ECU by ECU, as in the architecture */
static void print_core_table(const struct mp_design_point *point)
{
	const struct mp_params *params = &point->ctx.params;
	struct mp_table *table = mp_table_new(9);
	int id, i, j, max_id = -1;

	mp_table_header(table, 0, "core", false);
	mp_table_header(table, 1, "ECU", false);
	mp_table_header(table, 2, "type\nspeed %", true);
	mp_table_header(table, 3, "load\n%", true);
	mp_table_header(table, 4, "LET\n%", true);
	mp_table_header(table, 5, "scaling\nfactor", true);
	mp_table_header(table, 6, "sched", false);
	mp_table_header(table, 7, "tasks", true);
	mp_table_header(table, 8, "sub-chains\nchain.index", false);
	for (i = 0; i < point->cores.count; i++)
		if (point->cores.items[i].id > max_id)
			max_id = point->cores.items[i].id;
	for (id = 0; id <= max_id; id++) {
		const struct mp_core *core = find_core(&point->cores, id);
		char fragments[96] = "";
		size_t used = 0;
		int let_load, tasks;
		double factor;

		if (!core || core->is_empty)
			continue;
		core_contents(core, &let_load, &tasks);
		for (j = 0; j < core->chains.count; j++) {
			const struct mp_chain *chain = &core->chains.items[j];

			if (chain->is_let)
				continue;
			if (used + 16 >= sizeof(fragments)) {
				snprintf(fragments + used,
					 sizeof(fragments) - used, " ...");
				break;
			}
			used += (size_t)snprintf(fragments + used,
						 sizeof(fragments) - used,
						 "%s%d.%d", used ? " " : "",
						 chain->id,
						 chain->fragment_index);
		}
		factor = core->schedulable ? mp_core_scaling_factor(core) : -1;
		mp_table_row(table);
		mp_table_cell(table, 0, "c%d", core->id);
		mp_table_cell(table, 1, "%s",
			      mp_ecu_name(params, core->controller));
		mp_table_cell(table, 2, "%s %d", type_label(core->type),
			      core->speed);
		mp_table_cell(table, 3, "%.1f", percent(core->load));
		mp_table_cell(table, 4, "%.1f", percent(let_load));
		if (factor < 0)
			mp_table_cell(table, 5, "-");
		else
			mp_table_cell(table, 5, "%.3f", factor);
		mp_table_cell(table, 6, "%s", core->schedulable ? "yes" : "NO");
		mp_table_cell(table, 7, "%d", tasks);
		mp_table_cell(table, 8, "%s", fragments);
	}
	mp_table_print(table, stdout);
	mp_table_free(table);
}

/* why a task-chain is late, "" if on time */
static const char *why_late(const struct mp_chain_path *path)
{
	return path->on_time ? "" : !path->schedulable ? "task late" :
	       !path->network_ok ? "network" : "deadline";
}

static int network_hops(const struct mp_chain_path *path)
{
	int h, hops = 0;

	for (h = 0; h < path->hop_count; h++)
		hops += path->hops[h].type == MP_HOP_INTER_ECU;
	return hops;
}

/* the name of chain @id: from the file, else its number */
static const char *chain_name(const struct mp_names *names, int id, char *text,
			      size_t size)
{
	if (names && id >= 0 && id < names->chain_count && names->chain[id][0])
		return names->chain[id];
	snprintf(text, size, "%d", id);
	return text;
}

static void print_chain_table(const struct mp_design_point *point,
			      const struct mp_chain_array *application,
			      const struct mp_names *names)
{
	const struct mp_params *params = &point->ctx.params;
	struct mp_table *table = mp_table_new(9);
	int p, s;

	mp_table_header(table, 0, "chain", true);
	mp_table_header(table, 1, "tasks", true);
	mp_table_header(table, 2, "load\n%", true);
	mp_table_header(table, 3, "path: segments on cores\n(sub-chains)", false);
	mp_table_header(table, 4, "comm.\nECU/net", true);
	mp_table_header(table, 5, "reaction\nms", true);
	mp_table_header(table, 6, "deadline\nms", true);
	mp_table_header(table, 7, "on time", false);
	mp_table_header(table, 8, "why late", false);
	for (p = 0; p < point->paths.count; p++) {
		const struct mp_chain_path *path = &point->paths.items[p];
		const struct mp_chain *chain =
			&application->items[path->chain_id];
		char text[128] = "", core[48], number[16];
		size_t used = 0;
		int hops = network_hops(path);

		for (s = 0; s < path->segment_count; s++) {
			const struct mp_segment *segment = &path->segments[s];

			core_label(core, sizeof(core), params, &point->cores,
				   segment->core_id);
			if (used + 64 >= sizeof(text)) {
				snprintf(text + used, sizeof(text) - used,
					 " ...");
				break;
			}
			if (segment->first_fragment == segment->last_fragment)
				used += (size_t)snprintf(text + used,
					sizeof(text) - used, "%s%s(%d)",
					s ? " > " : "", core,
					segment->first_fragment);
			else
				used += (size_t)snprintf(text + used,
					sizeof(text) - used, "%s%s(%d-%d)",
					s ? " > " : "", core,
					segment->first_fragment,
					segment->last_fragment);
		}
		mp_table_row(table);
		mp_table_cell(table, 0, "%s", chain_name(names, path->chain_id,
							  number, sizeof(number)));
		mp_table_cell(table, 1, "%d", chain->tasks.count);
		mp_table_cell(table, 2, "%.2f", percent(chain->utilization));
		mp_table_cell(table, 3, "%s", text);
		mp_table_cell(table, 4, "%d/%d", path->hop_count - hops, hops);
		if (path->schedulable && path->network_ok)
			mp_table_cell(table, 5, "%.1f",
				      path->reaction_time / 1e3);
		else
			mp_table_cell(table, 5, "-");	/* no bound */
		if (path->deadline)
			mp_table_cell(table, 6, "%.1f", path->deadline / 1e3);
		else
			mp_table_cell(table, 6, "-");
		mp_table_cell(table, 7, "%s", path->on_time ? "yes" : "NO");
		mp_table_cell(table, 8, "%s", why_late(path));
	}
	mp_table_print(table, stdout);
	mp_table_free(table);
}

/* ---- result files (--out) ------------------------------------------------ */

/* a CSV field: quoted if it has a comma, a quote or a line break */
static void csv_text(FILE *out, const char *text)
{
	if (!strpbrk(text, ",\"\n")) {
		fputs(text, out);
		return;
	}
	fputc('"', out);
	for (; *text; text++) {
		if (*text == '"')
			fputc('"', out);
		fputc(*text, out);
	}
	fputc('"', out);
}

static FILE *open_output(const char *directory, const char *file)
{
	char path[4096];
	FILE *out;

	snprintf(path, sizeof(path), "%s/%s", directory, file);
	out = fopen(path, "w");
	if (!out)
		fprintf(stderr, "cannot write %s: %s\n", path, strerror(errno));
	return out;
}

/* the name of task @unique_id: from the file, else empty */
static const char *task_name(const struct mp_names *names, int unique_id)
{
	return names && unique_id >= 0 && unique_id < names->task_count ?
	       names->task[unique_id] : "";
}

static int write_mapping_csv(const char *directory, const struct run *runs,
			     int run_count, const struct mp_names *names)
{
	FILE *out = open_output(directory, "mapping.csv");
	char number[16];
	int a, i, j, k;

	if (!out)
		return -1;
	fprintf(out, "allocation,chain,chain_name,task,task_name,sub_chain,"
		"core,ecu,core_type,priority,period_us,wcet_us,"
		"response_time_us\n");
	for (a = 0; a < run_count; a++) {
		const struct mp_design_point *point = &runs[a].point;
		const struct mp_params *params = &point->ctx.params;

		if (!point->feasible)
			continue;
		for (i = 0; i < point->cores.count; i++) {
			const struct mp_core *core = &point->cores.items[i];

			for (j = 0; j < core->chains.count; j++) {
				const struct mp_chain *chain = &core->chains.items[j];

				if (chain->is_let)
					continue;
				for (k = 0; k < chain->tasks.count; k++) {
					const struct mp_task *task =
						&chain->tasks.items[k];

					fprintf(out, "%s,%d,",
						mp_algorithm_name(runs[a].algorithm),
						chain->id);
					csv_text(out, chain_name(names, chain->id,
								 number,
								 sizeof(number)));
					fprintf(out, ",%d,", task->id_in_chain);
					csv_text(out, task_name(names,
								task->unique_id));
					fprintf(out, ",%d,%d,", chain->fragment_index,
						core->id);
					csv_text(out, mp_ecu_name(params,
								  core->controller));
					fprintf(out, ",%s,%d,%d,%d,",
						mp_core_type_name(core->type),
						task->priority, task->period,
						task->wcet);
					/* above its period: only known to
					 * miss it, no WCRT */
					if (task->response_time <= task->period)
						fprintf(out, "%d",
							task->response_time);
					fprintf(out, "\n");
				}
			}
		}
	}
	fclose(out);
	return 0;
}

static int write_cores_csv(const char *directory, const struct run *runs,
			   int run_count)
{
	FILE *out = open_output(directory, "cores.csv");
	int a, i;

	if (!out)
		return -1;
	fprintf(out, "allocation,core,ecu,core_type,speed_percent,added,"
		"load_percent,let_load_percent,schedulable,"
		"critical_scaling_factor,tasks\n");
	for (a = 0; a < run_count; a++) {
		const struct mp_design_point *point = &runs[a].point;
		const struct mp_params *params = &point->ctx.params;

		if (!point->feasible)
			continue;
		for (i = 0; i < point->cores.count; i++) {
			const struct mp_core *core = &point->cores.items[i];
			int let_load, tasks;
			double factor;

			if (core->is_empty)
				continue;
			core_contents(core, &let_load, &tasks);
			factor = core->schedulable ?
				 mp_core_scaling_factor(core) : -1;
			fprintf(out, "%s,%d,", mp_algorithm_name(runs[a].algorithm),
				core->id);
			csv_text(out, mp_ecu_name(params, core->controller));
			fprintf(out, ",%s,%d,%s,%.2f,%.2f,%s,",
				mp_core_type_name(core->type), core->speed,
				core->id >= point->ctx.architecture_cores ? "yes" :
				"no", percent(core->load), percent(let_load),
				core->schedulable ? "yes" : "no");
			if (factor >= 0)
				fprintf(out, "%.4f", factor);
			fprintf(out, ",%d\n", tasks);
		}
	}
	fclose(out);
	return 0;
}

static int write_ecus_csv(const char *directory, const struct run *runs,
			  int run_count)
{
	FILE *out = open_output(directory, "ecus.csv");
	int a, c;

	if (!out)
		return -1;
	fprintf(out, "allocation,ecu,role,cores_used,application_load_cores,"
		"let_load_cores,critical_scaling_factor,memory_kb,"
		"memory_capacity_kb,egress_port_load_percent,"
		"ingress_port_load_percent\n");
	for (a = 0; a < run_count; a++) {
		const struct mp_design_point *point = &runs[a].point;
		const struct mp_params *params = &point->ctx.params;
		const struct mp_network_summary *net =
			&point->communication.network;

		if (!point->feasible)
			continue;
		for (c = 0; c < mp_ecu_count(params); c++) {
			const struct mp_controller_stats *ecu =
				&point->stats.controllers[c];

			fprintf(out, "%s,", mp_algorithm_name(runs[a].algorithm));
			csv_text(out, mp_ecu_name(params, c));
			fprintf(out, ",%s,%d,%.4f,%.4f,",
				mp_role_name(mp_ecu_role(params, c)),
				ecu->used_cores, ecu->application_utilization,
				ecu->let_utilization);
			if (ecu->scaling_factor >= 0)
				fprintf(out, "%.4f", ecu->scaling_factor);
			fprintf(out, ",%d,%d,%.2f,%.2f\n", ecu->memory,
				mp_ecu_memory(params, c),
				net->egress_utilization[c],
				net->ingress_utilization[c]);
		}
	}
	fclose(out);
	return 0;
}

static int write_chains_csv(const char *directory, const struct run *runs,
			    int run_count,
			    const struct mp_chain_array *application,
			    const struct mp_names *names)
{
	FILE *out = open_output(directory, "chains.csv");
	char number[16];
	int a, p;

	if (!out)
		return -1;
	fprintf(out, "allocation,chain,name,tasks,utilization_percent,"
		"sub_chains,inter_ecu_communications,intra_ecu_communications,"
		"end_to_end_response_time_us,max_reaction_time_us,deadline_us,"
		"schedulable,network_ok,on_time,why_late\n");
	for (a = 0; a < run_count; a++) {
		const struct mp_design_point *point = &runs[a].point;

		if (!point->feasible)
			continue;
		for (p = 0; p < point->paths.count; p++) {
			const struct mp_chain_path *path = &point->paths.items[p];
			const struct mp_chain *chain =
				&application->items[path->chain_id];
			int hops = network_hops(path);

			fprintf(out, "%s,%d,", mp_algorithm_name(runs[a].algorithm),
				path->chain_id);
			csv_text(out, chain_name(names, path->chain_id, number,
						 sizeof(number)));
			fprintf(out, ",%d,%.4f,%d,%d,%d,", chain->tasks.count,
				percent(chain->utilization), path->fragment_count,
				hops, path->hop_count - hops);
			/* no bound for a late task or an unbounded port */
			if (path->schedulable && path->network_ok)
				fprintf(out, "%d,%d,", path->latency,
					path->reaction_time);
			else
				fprintf(out, ",,");
			if (path->deadline)
				fprintf(out, "%d", path->deadline);
			fprintf(out, ",%s,%s,%s,%s\n",
				path->schedulable ? "yes" : "no",
				path->network_ok ? "yes" : "no",
				path->on_time ? "yes" : "no", why_late(path));
		}
	}
	fclose(out);
	return 0;
}

static int write_summary_json(const char *directory, const struct run *runs,
			      int run_count,
			      const struct mp_chain_array *application)
{
	FILE *out = open_output(directory, "summary.json");
	int a, i, tasks = 0;

	if (!out)
		return -1;
	for (i = 0; i < application->count; i++)
		tasks += application->items[i].tasks.count;
	fprintf(out, "{\n  \"task_chains\": %d,\n  \"tasks\": %d,\n"
		"  \"runs\": [\n", application->count, tasks);
	for (a = 0; a < run_count; a++) {
		const struct mp_design_point *p = &runs[a].point;
		const struct mp_communication_summary *comm = &p->communication;

		fprintf(out, "    {\"allocation\": \"%s\", \"feasible\": %s",
			mp_algorithm_name(runs[a].algorithm),
			p->feasible ? "true" : "false");
		if (p->feasible) {
			fprintf(out, ",\n     \"sub_chains\": %d, "
				"\"fragmentation_rate_percent\": %.2f,\n"
				"     \"cores_used\": %d, \"central_cores\": %d, "
				"\"other_cores\": %d, \"lower_bound_cores\": %d, "
				"\"cores_added\": %d, \"cores_emptied\": %d,\n"
				"     \"schedulable_cores_percent\": %.2f, "
				"\"critical_scaling_factor\": %.4f,\n"
				"     \"application_load_cores\": %.4f, "
				"\"let_load_cores\": %.4f,\n"
				"     \"chains_schedulable_percent\": %.2f, "
				"\"chains_on_time_percent\": %.2f, "
				"\"deadline_misses\": %d, "
				"\"local_search_moves\": %d,\n"
				"     \"intra_ecu_communications\": %d, "
				"\"inter_ecu_communications\": %d, "
				"\"frer_flows\": %d,\n"
				"     \"network_delay_max_us\": %.3f, "
				"\"busiest_port_percent\": %.3f, "
				"\"overloaded_ports\": %d,\n"
				"     \"end_to_end_response_time_max_us\": %.1f, "
				"\"max_reaction_time_max_us\": %.1f,\n"
				"     \"cpu_time_ms\": %.3f",
				p->fragments.count, p->stats.fragmentation_rate,
				p->stats.used_cores, p->stats.central_cores,
				p->stats.other_cores, p->stats.lower_bound_cores,
				p->cores.count - p->ctx.architecture_cores,
				p->stats.emptied_cores, p->stats.rate_final,
				p->stats.scaling_factor,
				p->stats.application_utilization,
				p->stats.let_utilization,
				comm->schedulable_chain_rate,
				comm->on_time_chain_rate, comm->deadline_misses,
				p->stats.deadline_repairs, comm->intra_ecu_hops,
				comm->inter_ecu_hops, comm->network.frer_flows,
				comm->network.max_delay,
				comm->network.max_port_utilization,
				comm->network.overloaded_ports,
				comm->max_latency, comm->max_reaction_time,
				p->times.total * 1e3);
			if (p->ctx.params.target_scaling)
				fprintf(out, ",\n     \"sizing_cap\": %.4f, "
					"\"sizing_target_met\": %s",
					p->sizing.fill_limit /
					(double)MP_UTILIZATION_UNIT,
					p->sizing.target_met ? "true" : "false");
		}
		fprintf(out, "}%s\n", a + 1 < run_count ? "," : "");
	}
	fprintf(out, "  ]\n}\n");
	fclose(out);
	return 0;
}

/* a placed fragment, to follow a chain core by core */
struct placed {
	int chain, index;
	const struct mp_core *core;
	const struct mp_chain *fragment;
};

static int placed_compare(const void *a, const void *b)
{
	const struct placed *x = a, *y = b;

	if (x->chain != y->chain)
		return x->chain < y->chain ? -1 : 1;
	return (x->index > y->index) - (x->index < y->index);
}

/* the network bound of the hop of @path after fragment @index, or 0 */
static double hop_bound(const struct mp_chain_path *path, int index)
{
	int h;

	for (h = 0; h < path->hop_count; h++)
		if (path->hops[h].last_source_fragment == index)
			return path->hops[h].type == MP_HOP_INTER_ECU ?
			       path->hops[h].flow.total_delay : 0;
	return 0;
}

/*
 * system.json: the timing model of each mapping, for the full-system
 * simulation (tests/syssim): every core with its LET task and its tasks,
 * every task-chain with the core of each task, the network bound of each
 * hop and the bounds of the analysis
 */
static int write_system_json(const char *directory, const struct run *runs,
			     int run_count)
{
	FILE *out = open_output(directory, "system.json");
	int a, i, j, k;

	if (!out)
		return -1;
	fprintf(out, "{\"runs\": [\n");
	for (a = 0; a < run_count; a++) {
		const struct mp_design_point *point = &runs[a].point;
		struct placed *placed;
		int count = 0, first = 1, p, n;

		fprintf(out, "%s {\"allocation\": \"%s\", \"feasible\": %s",
			a ? "," : "", mp_algorithm_name(runs[a].algorithm),
			point->feasible ? "true" : "false");
		if (!point->feasible) {
			fprintf(out, "}\n");
			continue;
		}
		fprintf(out, ",\n  \"communication\": \"%s\",\n  \"cores\": [\n",
			point->ctx.params.network.strict_let ? "LET" :
			"implicit");
		for (i = 0; i < point->cores.count; i++) {
			const struct mp_core *core = &point->cores.items[i];
			int let_period = 0, let_wcet = 0, t = 0;

			if (core->is_empty)
				continue;
			for (j = 0; j < core->chains.count; j++)
				if (core->chains.items[j].is_let &&
				    core->chains.items[j].tasks.count) {
					let_period = core->chains.items[j].tasks.items[0].period;
					let_wcet = core->chains.items[j].tasks.items[0].wcet;
				}
			fprintf(out, "%s   {\"id\": %d, \"ecu\": %d, "
				"\"let_period_us\": %d, \"let_wcet_us\": %d, "
				"\"tasks\": [", first ? "" : ",\n", core->id,
				core->controller, let_period, let_wcet);
			first = 0;
			for (j = 0; j < core->chains.count; j++) {
				const struct mp_chain *chain = &core->chains.items[j];

				if (chain->is_let)
					continue;
				for (k = 0; k < chain->tasks.count; k++) {
					const struct mp_task *task =
						&chain->tasks.items[k];

					fprintf(out, "%s\n    {\"chain\": %d, "
						"\"task\": %d, \"period_us\": %d, "
						"\"wcet_us\": %d, \"priority\": %d, "
						"\"response_time_us\": ",
						t++ ? "," : "", chain->id,
						task->id_in_chain, task->period,
						task->wcet, task->priority);
					if (task->response_time <= task->period)
						fprintf(out, "%d}",
							task->response_time);
					else
						fprintf(out, "null}");
				}
			}
			fprintf(out, "]}");
		}
		fprintf(out, "\n  ],\n  \"chains\": [\n");
		placed = malloc(((size_t)point->fragments.count + 1) *
				sizeof(*placed));
		if (!placed) {
			fclose(out);
			return -1;
		}
		for (i = 0; i < point->cores.count; i++)
			for (j = 0; j < point->cores.items[i].chains.count; j++) {
				const struct mp_chain *chain =
					&point->cores.items[i].chains.items[j];

				if (chain->is_let || count > point->fragments.count)
					continue;
				placed[count].chain = chain->id;
				placed[count].index = chain->fragment_index;
				placed[count].core = &point->cores.items[i];
				placed[count++].fragment = chain;
			}
		qsort(placed, (size_t)count, sizeof(*placed), placed_compare);
		for (p = 0, n = 0; p < point->paths.count; p++) {
			const struct mp_chain_path *path = &point->paths.items[p];
			int t = 0;

			fprintf(out, "%s   {\"id\": %d, \"deadline_us\": %d, "
				"\"end_to_end_bound_us\": %d, "
				"\"reaction_bound_us\": %d, \"bounded\": %s, "
				"\"on_time\": %s, \"tasks\": [", p ? ",\n" : "",
				path->chain_id, path->deadline, path->e2e_delay,
				path->reaction_time,
				path->schedulable && path->network_ok ? "true" :
				"false", path->on_time ? "true" : "false");
			while (n < count && placed[n].chain < path->chain_id)
				n++;
			for (; n < count && placed[n].chain == path->chain_id; n++) {
				const struct mp_chain *f = placed[n].fragment;

				for (k = 0; k < f->tasks.count; k++)
					fprintf(out, "%s\n    {\"task\": %d, "
						"\"core\": %d, \"hop_bound_us\": %.3f}",
						t++ ? "," : "",
						f->tasks.items[k].id_in_chain,
						placed[n].core->id,
						k + 1 == f->tasks.count ?
						hop_bound(path, placed[n].index) : 0.0);
			}
			fprintf(out, "]}");
		}
		free(placed);
		fprintf(out, "\n  ]}\n");
	}
	fprintf(out, "]}\n");
	fclose(out);
	return 0;
}

static int write_outputs(const char *directory, const struct run *runs,
			 int run_count, const struct mp_chain_array *application,
			 const struct mp_names *names)
{
	if (mkdir(directory, 0777) && errno != EEXIST) {
		fprintf(stderr, "cannot create %s: %s\n", directory,
			strerror(errno));
		return -1;
	}
	if (write_mapping_csv(directory, runs, run_count, names) ||
	    write_cores_csv(directory, runs, run_count) ||
	    write_ecus_csv(directory, runs, run_count) ||
	    write_chains_csv(directory, runs, run_count, application, names) ||
	    write_summary_json(directory, runs, run_count, application) ||
	    write_system_json(directory, runs, run_count))
		return -1;
	return 0;
}

/* ---- the input, as the tool understood it -------------------------------- */

static void print_platform(const struct mp_params *params)
{
	int c, t;

	printf("  platform: %d ECUs\n", mp_ecu_count(params));
	for (c = 0; c < mp_ecu_count(params); c++) {
		struct mp_ecu ecu;
		bool first = true;

		mp_ecu_get(params, c, &ecu);
		printf("    %-12s %-8s ", ecu.name, mp_role_name(ecu.role));
		for (t = 0; t < MP_CORE_TYPES; t++)
			if (ecu.cores[t] || (ecu.speed[t] && t == ecu.added_type)) {
				printf("%s%d %s at %d%%", first ? "" : ", ",
				       ecu.cores[t], type_label(t), ecu.speed[t]);
				first = false;
			}
		if (ecu.memory)
			printf(", %d KB", ecu.memory);
		if (ecu.max_cores)
			printf(", at most %d cores", ecu.max_cores);
		printf("\n");
	}
}

static void print_input(const struct mp_ctx *ctx,
			const struct mp_chain_array *application,
			const char *origin)
{
	const struct mp_params *params = &ctx->params;
	const struct mp_net_config *net = &params->network;
	int i, tasks = 0, local = 0, bound_first = 0, bound_last = 0;

	for (i = 0; i < application->count; i++) {
		const struct mp_chain *chain = &application->items[i];

		tasks += chain->tasks.count;
		local += chain->controller != MP_CONTROLLER_ANY;
		bound_first += chain->controller == MP_CONTROLLER_ANY &&
			chain->tasks.items[0].controller != MP_CONTROLLER_ANY;
		bound_last += chain->controller == MP_CONTROLLER_ANY &&
			chain->tasks.items[chain->tasks.count - 1].controller !=
			MP_CONTROLLER_ANY;
	}
	printf("\n  %s: %d task-chains, %d tasks\n", origin, application->count,
	       tasks);
	printf("  %d local to one ECU; of the others, %d start at a bound "
	       "sensor task, %d end at a bound actuator task\n",
	       local, bound_first, bound_last);
	print_platform(params);
	printf("  execution: fixed-priority preemptive, %s communication at "
	       "LET instants, utilization cap %.2f, LET task %d us, WCET "
	       "margin %d%%\n", params->network.strict_let ? "strict LET" :
	       "implicit", params->fill_limit / (double)MP_UTILIZATION_UNIT,
	       params->let_wcet, params->wcet_margin);
	if (params->memory_latency)
		printf("  interference: shared memory, %d ns per request, %d "
		       "requests per ms of WCET, round robin\n",
		       params->memory_latency, params->access_density);
	printf("  network: %s, %d Mbit/s (backbone %d)%s, %d %s class%s%s\n",
	       mp_net_topology_name(net->topology), net->link_mbps,
	       net->backbone_mbps ? net->backbone_mbps : net->link_mbps,
	       net->preemption ? ", preemption" : "", net->class_count,
	       mp_net_shaping_name(net->shaping),
	       net->class_count > 1 ? "es" : "", net->frer ? ", FRER" : "");
	printf("  method: sigma %.2f%s%s%s%s%s%s\n\n",
	       params->sigma / (double)MP_UTILIZATION_UNIT,
	       params->affinity ? "" : ", no affinity",
	       params->consolidate ? "" : ", no consolidation",
	       params->swap_moves ? ", swap moves" : "",
	       params->relocation_moves ? "" : ", no relocation moves",
	       params->repair_deadlines ? "" : ", no local search",
	       params->target_scaling ? ", sized for a critical scaling factor" :
	       params->fixed_hardware ? ", fixed hardware" : "");
}

/* ---- command line ------------------------------------------------------ */

static void usage(FILE *out, const char *program)
{
	fprintf(out,
		"usage: %s [APPLICATION.json [CONFIGURATION.json]] [options]\n\n"
		"Without APPLICATION, a generated application (--seed, --chains).\n"
		"Without CONFIGURATION, the built-in zonal platform and the default\n"
		"method; the four allocation rules are then compared. With it, the\n"
		"rule of the file (--algorithm all compares them). Options override\n"
		"the configuration file.\n\n"
		"files\n"
		"  --config FILE     configuration file (instead of the second argument)\n"
		"  --example-config [T]  print the configuration of template T: zonal\n"
		"                    (default), domain, hybrid or distributed, then exit\n"
		"  --example-application  print a small application file, then exit\n"
		"  --generate        print the generated application file, then exit\n"
		"  --print-config    print the configuration in effect, then exit\n"
		"  --out DIR         write mapping.csv, chains.csv, cores.csv, ecus.csv,\n"
		"                    summary.json and system.json into DIR\n"
		"generated application\n"
		"  --seed N          seed of the generator (default: $MP_SEED or 1)\n"
		"  --chains N        task-chains (300)\n"
		"  --detection-chain the detection chain (9 tasks) instead\n"
		"  --replicas P      %% of the chains that are replicas of others (0)\n"
		"  --colocate P      %% of consecutive free tasks that share a core (0)\n"
		"  --zone-only P     %% of chains whose free tasks avoid the central\n"
		"                    computers (0)\n"
		"  --lockstep P      %% of chains (safety) on lock-step cores only (0)\n"
		"  --type-variation P  WCET on each core type within +-P%% of its speed ratio (0)\n"
		"method\n"
		"  --algorithm A     BFDU, WFDU, FFDU, CADU or all\n"
		"  --sigma S         utilization limit of a sub-chain, fraction of a core (0.4)\n"
		"  --target-scaling F  sizing: lower the cap until the critical scaling\n"
		"                    factor reaches F, e.g. 1.25 (none)\n"
		"  --no-affinity     allocation ignores the other sub-chains of a chain\n"
		"  --no-repair       no deadline-driven local search\n"
		"  --no-consolidation  keep every core the allocation opened\n"
		"  --swap            also try swap moves between unschedulable cores\n"
		"  --no-relocation   no relocation moves from unschedulable cores\n"
		"  --fixed-hardware  add no core: a sub-chain that fits nowhere makes\n"
		"                    the mapping infeasible\n"
		"  --separate-cores  replicas on different cores, not ECUs\n"
		"execution model\n"
		"  --phi F           utilization cap of a core, fraction (0.8)\n"
		"  --let C           WCET of the LET tasks on the reference core, us (50)\n"
		"  --strict-let      strict LET between cores: an output is published at\n"
		"                    the end of its producer's period (default: implicit,\n"
		"                    copied at the first LET instant after it ends)\n"
		"  --margin P        WCET margin in percent (0)\n"
		"interference model\n"
		"  --memory-latency L  shared memory of each ECU: L ns per request,\n"
		"                    round robin between its cores (0: no contention)\n"
		"  --access-density N  memory requests per ms of reference WCET (1000)\n"
		"network model\n"
		"  --topology T      star, tree or ring (star)\n"
		"  --link M          rate of the ECU links, Mbit/s (1000)\n"
		"  --backbone M      rate of the links between switches, Mbit/s (link)\n"
		"  --classes N       LET traffic classes by period, 1 to 3 (1)\n"
		"  --tas             time-aware shaping instead of credit-based shapers\n"
		"  --ats             asynchronous traffic shaping (802.1Qcr) instead\n"
		"  --preemption      frame preemption on the links\n"
		"  --frer            ring or custom topology: messages of replicated\n"
		"                    task-chains over two routes (802.1CB)\n"
		"built-in zonal platform (not with a platform in the configuration)\n"
		"  --pcu-cores N     big cores of the central computer, PCU (8)\n"
		"  --pcu-little N    little cores of the PCU (0)\n"
		"  --pcu-speed P     speed of a big core, %% of the reference (100)\n"
		"  --little-speed P  speed of a little core, %% of the reference (40)\n"
		"  --pcu-memory K    memory of the PCU for the tasks, KB (unlimited)\n"
		"  --zcu-cores N     microcontroller cores of each zone controller (4)\n"
		"  --zcu-lockstep N  lock-step cores of each zone controller (0)\n"
		"  --zcu-speed P     speed of a zone core, %% of the reference (50)\n"
		"  --zcu-memory K    memory of each zone controller, KB (unlimited)\n"
		"output\n"
		"  --cores           also print the mapping: cores and task-chains\n"
		"  --help            this text\n",
		program);
}

/* a fraction such as 0.4 as ppm, or -1 */
static int parse_fraction(const char *text)
{
	double value = strtod(text, NULL);

	return value > 0 && value <= 1 ? (int)(value * MP_UTILIZATION_UNIT +
					       0.5) : -1;
}

/* a small application for the zonal platform (--example-application) */
static const char example_application[] =
"{\n"
"  \"description\": \"three task-chains on the built-in zonal platform\",\n"
"  \"chains\": [\n"
"    {\"name\": \"brake_by_wire\", \"deadline_us\": 10000, \"tasks\": [\n"
"      {\"name\": \"pedal_sensor\", \"period_us\": 1000, \"wcet_us\": 40,\n"
"       \"ecu\": \"ZCU1\", \"message_bytes\": 16},\n"
"      {\"name\": \"brake_control\", \"period_us\": 1000, \"wcet_us\": 120,\n"
"       \"message_bytes\": 32},\n"
"      {\"name\": \"caliper_front_left\", \"period_us\": 1000, \"wcet_us\": 30,\n"
"       \"ecu\": \"ZCU2\"}\n"
"    ]},\n"
"    {\"name\": \"lane_keeping\", \"deadline_us\": 100000, \"tasks\": [\n"
"      {\"name\": \"camera\", \"period_us\": 33000, \"wcet_us\": 2000,\n"
"       \"ecu\": \"ZCU1\", \"message_bytes\": 4000},\n"
"      {\"name\": \"lane_detection\", \"period_us\": 33000, \"wcet_us\": 9000,\n"
"       \"core_types\": [\"big\"], \"memory_kb\": 2048, \"message_bytes\": 64},\n"
"      {\"name\": \"steering_request\", \"period_us\": 10000, \"wcet_us\": 200,\n"
"       \"ecu\": \"ZCU3\"}\n"
"    ]},\n"
"    {\"name\": \"window_lifter\", \"ecu\": \"ZCU4\", \"tasks\": [\n"
"      {\"name\": \"switch\", \"period_us\": 20000, \"wcet_us\": 50},\n"
"      {\"name\": \"motor\", \"period_us\": 20000, \"wcet_us\": 100}\n"
"    ]}\n"
"  ]\n"
"}\n";

enum option_id {
	OPT_SEED = 256, OPT_CHAINS, OPT_DETECTION, OPT_SIGMA,
	OPT_ALGORITHM, OPT_MARGIN, OPT_NO_AFFINITY, OPT_NO_REPAIR,
	OPT_PHI, OPT_PCU_SPEED, OPT_ZCU_SPEED, OPT_LET, OPT_LINK,
	OPT_PREEMPTION, OPT_CORES, OPT_HELP, OPT_PCU_CORES,
	OPT_ZCU_CORES, OPT_TOPOLOGY, OPT_BACKBONE, OPT_CLASSES, OPT_TAS,
	OPT_ATS, OPT_FRER, OPT_MEMORY_LATENCY, OPT_ACCESS_DENSITY,
	OPT_NO_CONSOLIDATION, OPT_REPLICAS, OPT_COLOCATE, OPT_ZONE_ONLY,
	OPT_PCU_MEMORY, OPT_ZCU_MEMORY, OPT_SEPARATE_CORES, OPT_LOCKSTEP,
	OPT_VARIATION, OPT_PCU_LITTLE, OPT_LITTLE_SPEED, OPT_ZCU_LOCKSTEP,
	OPT_TARGET, OPT_FIXED_HARDWARE, OPT_CONFIG, OPT_EXAMPLE_CONFIG,
	OPT_EXAMPLE_APPLICATION, OPT_GENERATE, OPT_PRINT_CONFIG, OPT_OUT,
	OPT_SWAP, OPT_NO_RELOCATION, OPT_STRICT_LET,
};

static const struct option options[] = {
	{"seed", required_argument, NULL, OPT_SEED},
	{"chains", required_argument, NULL, OPT_CHAINS},
	{"detection-chain", no_argument, NULL, OPT_DETECTION},
	{"sigma", required_argument, NULL, OPT_SIGMA},
	{"algorithm", required_argument, NULL, OPT_ALGORITHM},
	{"margin", required_argument, NULL, OPT_MARGIN},
	{"target-scaling", required_argument, NULL, OPT_TARGET},
	{"no-affinity", no_argument, NULL, OPT_NO_AFFINITY},
	{"no-repair", no_argument, NULL, OPT_NO_REPAIR},
	{"no-consolidation", no_argument, NULL, OPT_NO_CONSOLIDATION},
	{"swap", no_argument, NULL, OPT_SWAP},
	{"no-relocation", no_argument, NULL, OPT_NO_RELOCATION},
	{"fixed-hardware", no_argument, NULL, OPT_FIXED_HARDWARE},
	{"phi", required_argument, NULL, OPT_PHI},
	{"pcu-cores", required_argument, NULL, OPT_PCU_CORES},
	{"zcu-cores", required_argument, NULL, OPT_ZCU_CORES},
	{"pcu-speed", required_argument, NULL, OPT_PCU_SPEED},
	{"zcu-speed", required_argument, NULL, OPT_ZCU_SPEED},
	{"let", required_argument, NULL, OPT_LET},
	{"strict-let", no_argument, NULL, OPT_STRICT_LET},
	{"link", required_argument, NULL, OPT_LINK},
	{"preemption", no_argument, NULL, OPT_PREEMPTION},
	{"topology", required_argument, NULL, OPT_TOPOLOGY},
	{"backbone", required_argument, NULL, OPT_BACKBONE},
	{"classes", required_argument, NULL, OPT_CLASSES},
	{"tas", no_argument, NULL, OPT_TAS},
	{"ats", no_argument, NULL, OPT_ATS},
	{"frer", no_argument, NULL, OPT_FRER},
	{"memory-latency", required_argument, NULL, OPT_MEMORY_LATENCY},
	{"access-density", required_argument, NULL, OPT_ACCESS_DENSITY},
	{"replicas", required_argument, NULL, OPT_REPLICAS},
	{"colocate", required_argument, NULL, OPT_COLOCATE},
	{"zone-only", required_argument, NULL, OPT_ZONE_ONLY},
	{"pcu-memory", required_argument, NULL, OPT_PCU_MEMORY},
	{"zcu-memory", required_argument, NULL, OPT_ZCU_MEMORY},
	{"separate-cores", no_argument, NULL, OPT_SEPARATE_CORES},
	{"lockstep", required_argument, NULL, OPT_LOCKSTEP},
	{"type-variation", required_argument, NULL, OPT_VARIATION},
	{"pcu-little", required_argument, NULL, OPT_PCU_LITTLE},
	{"little-speed", required_argument, NULL, OPT_LITTLE_SPEED},
	{"zcu-lockstep", required_argument, NULL, OPT_ZCU_LOCKSTEP},
	{"config", required_argument, NULL, OPT_CONFIG},
	{"example-config", optional_argument, NULL, OPT_EXAMPLE_CONFIG},
	{"example-application", no_argument, NULL, OPT_EXAMPLE_APPLICATION},
	{"generate", no_argument, NULL, OPT_GENERATE},
	{"print-config", no_argument, NULL, OPT_PRINT_CONFIG},
	{"out", required_argument, NULL, OPT_OUT},
	{"cores", no_argument, NULL, OPT_CORES},
	{"help", no_argument, NULL, OPT_HELP},
	{NULL, 0, NULL, 0},
};

static const char *option_name(int option)
{
	int i;

	for (i = 0; options[i].name; i++)
		if (options[i].val == option)
			return options[i].name;
	return "?";
}

/* the options of the built-in zonal platform */
static bool is_platform_option(int option)
{
	switch (option) {
	case OPT_PCU_CORES: case OPT_ZCU_CORES: case OPT_PCU_SPEED:
	case OPT_ZCU_SPEED: case OPT_PCU_MEMORY: case OPT_ZCU_MEMORY:
	case OPT_PCU_LITTLE: case OPT_LITTLE_SPEED: case OPT_ZCU_LOCKSTEP:
		return true;
	default:
		return false;
	}
}

struct settings {
	struct mp_ctx ctx;
	struct mp_gen_params generator;
	uint64_t seed;
	bool detection, show_cores, generate, print_config;
	const char *out;
	bool one_rule;			/* else the four rules */
	enum mp_algorithm algorithm;	/* the one rule */
};

/* apply one option to @s (the configuration file is already read) */
static int apply_option(struct settings *s, int option, const char *arg)
{
	struct mp_params *params = &s->ctx.params;
	char *end;
	long value;

	switch (option) {
	case OPT_SEED:
		s->seed = strtoull(arg, NULL, 10);
		return 0;
	case OPT_DETECTION:
		s->detection = true;
		return 0;
	case OPT_SIGMA:
		params->sigma = parse_fraction(arg);
		return 0;
	case OPT_PHI:
		params->fill_limit = parse_fraction(arg);
		return 0;
	case OPT_ALGORITHM:
		if (!strcmp(arg, "all")) {
			s->one_rule = false;
			return 0;
		}
		if (mp_parse_algorithm(arg, &s->algorithm)) {
			fprintf(stderr, "unknown algorithm %s\n", arg);
			return -1;
		}
		s->one_rule = true;
		return 0;
	case OPT_TARGET:
		params->target_scaling = (int)(strtod(arg, NULL) * 1000 + 0.5);
		return 0;
	case OPT_NO_AFFINITY:
		params->affinity = false;
		return 0;
	case OPT_NO_REPAIR:
		params->repair_deadlines = false;
		return 0;
	case OPT_NO_CONSOLIDATION:
		params->consolidate = false;
		return 0;
	case OPT_SWAP:
		params->swap_moves = true;
		return 0;
	case OPT_NO_RELOCATION:
		params->relocation_moves = false;
		return 0;
	case OPT_STRICT_LET:
		params->network.strict_let = true;
		return 0;
	case OPT_FIXED_HARDWARE:
		params->fixed_hardware = true;
		return 0;
	case OPT_SEPARATE_CORES:
		params->separation = MP_SEPARATE_CORES;
		return 0;
	case OPT_PREEMPTION:
		params->network.preemption = true;
		return 0;
	case OPT_TAS:
		params->network.shaping = MP_NET_TAS;
		return 0;
	case OPT_ATS:
		params->network.shaping = MP_NET_ATS;
		return 0;
	case OPT_FRER:
		params->network.frer = true;
		return 0;
	case OPT_TOPOLOGY:
		if (mp_net_parse_topology(arg, &params->network.topology)) {
			fprintf(stderr, "unknown topology %s (star, tree or ring; "
				"a custom one in a configuration file)\n", arg);
			return -1;
		}
		return 0;
	case OPT_CORES:
		s->show_cores = true;
		return 0;
	case OPT_GENERATE:
		s->generate = true;
		return 0;
	case OPT_PRINT_CONFIG:
		s->print_config = true;
		return 0;
	case OPT_OUT:
		s->out = arg;
		return 0;
	default:
		break;
	}
	/* the options with an integer */
	value = strtol(arg, &end, 10);
	if (!*arg || *end || value < -1000000000 || value > 1000000000) {
		fprintf(stderr, "--%s: not an integer: %s\n", option_name(option),
			arg);
		return -1;
	}
	switch (option) {
	case OPT_CHAINS: s->generator.chain_count = (int)value; break;
	case OPT_MARGIN: params->wcet_margin = (int)value; break;
	case OPT_LET: params->let_wcet = (int)value; break;
	case OPT_LINK: params->network.link_mbps = (int)value; break;
	case OPT_BACKBONE: params->network.backbone_mbps = (int)value; break;
	case OPT_CLASSES:
		if (value < 1 || value > MP_NET_MAX_CLASSES) {
			fprintf(stderr, "--classes: 1 to %d\n", MP_NET_MAX_CLASSES);
			return -1;
		}
		mp_net_set_classes(&params->network, (int)value);
		break;
	case OPT_MEMORY_LATENCY: params->memory_latency = (int)value; break;
	case OPT_ACCESS_DENSITY: params->access_density = (int)value; break;
	case OPT_REPLICAS: s->generator.replica_percent = (int)value; break;
	case OPT_COLOCATE: s->generator.colocate_percent = (int)value; break;
	case OPT_ZONE_ONLY: s->generator.zone_only_percent = (int)value; break;
	case OPT_LOCKSTEP: s->generator.lockstep_percent = (int)value; break;
	case OPT_VARIATION: s->generator.type_variation = (int)value; break;
	case OPT_PCU_CORES: params->pcu_cores = (int)value; break;
	case OPT_ZCU_CORES: params->zcu_cores = (int)value; break;
	case OPT_PCU_SPEED: params->pcu_speed = (int)value; break;
	case OPT_ZCU_SPEED: params->zcu_speed = (int)value; break;
	case OPT_PCU_MEMORY: params->pcu_memory = (int)value; break;
	case OPT_ZCU_MEMORY: params->zcu_memory = (int)value; break;
	case OPT_PCU_LITTLE: params->pcu_little_cores = (int)value; break;
	case OPT_LITTLE_SPEED: params->little_speed = (int)value; break;
	case OPT_ZCU_LOCKSTEP: params->zcu_lockstep_cores = (int)value; break;
	default: return -1;
	}
	return 0;
}

static bool settings_valid(const struct settings *s)
{
	const struct mp_params *params = &s->ctx.params;
	char message[160];

	if (params->sigma < 0 || params->fill_limit < 0 ||
	    params->sigma > params->fill_limit ||
	    params->pcu_speed < 1 || params->zcu_speed < 1 ||
	    params->little_speed < 1 || params->pcu_cores < 0 ||
	    params->zcu_cores < 0 || params->pcu_little_cores < 0 ||
	    params->zcu_lockstep_cores < 0 || params->pcu_memory < 0 ||
	    params->zcu_memory < 0 || params->let_wcet < 0 ||
	    params->wcet_margin < 0 || params->memory_latency < 0 ||
	    params->access_density < 0 || params->target_scaling < 0 ||
	    s->generator.chain_count < 1 || s->generator.replica_percent < 0 ||
	    s->generator.replica_percent > 50 ||
	    s->generator.type_variation < 0 ||
	    s->generator.type_variation > 300 ||
	    mp_net_config_check(&params->network)) {
		fprintf(stderr, "invalid option value: 0 < sigma <= phi <= 1, "
			"speeds, link rates and chains positive, core counts "
			"and memories not negative, --frer on a ring or a "
			"custom topology\n");
		return false;
	}
	if (mp_platform_check(params, message, sizeof(message))) {
		fprintf(stderr, "invalid platform: %s\n", message);
		return false;
	}
	return true;
}

int main(int argc, char **argv)
{
	static const enum mp_algorithm all[ALGORITHMS] = {
		MP_BFDU, MP_WFDU, MP_FFDU, MP_CADU,
	};
	struct { int option; const char *arg; } given[128];
	const char *env_seed = getenv("MP_SEED");
	const char *application_path = NULL, *config_path = NULL;
	struct run runs[ALGORITHMS];
	struct mp_chain_array application;
	struct mp_names names = {0};
	struct settings s;
	char error[512], origin[600];
	int given_count = 0, run_count, option, a, i;
	bool platform_option = false;

	memset(&s, 0, sizeof(s));
	s.seed = env_seed ? strtoull(env_seed, NULL, 10) : 1;
	mp_gen_params_default(&s.generator);

	/* first the options that end the program, and the files */
	while ((option = getopt_long(argc, argv, "", options, NULL)) != -1) {
		switch (option) {
		case OPT_HELP:
			usage(stdout, argv[0]);
			return 0;
		case OPT_EXAMPLE_CONFIG: {
			struct mp_params params;
			const char *name = optarg ? optarg : "zonal";

			/* "--example-config domain" as well as "=domain" */
			if (!optarg && optind < argc && argv[optind][0] != '-')
				name = argv[optind++];
			if (mp_params_template(&params, name)) {
				fprintf(stderr, "unknown template %s (zonal, domain, "
					"hybrid or distributed)\n", name);
				return 2;
			}
			mp_write_configuration(stdout, &params);
			return 0;
		}
		case OPT_EXAMPLE_APPLICATION:
			fputs(example_application, stdout);
			return 0;
		case OPT_CONFIG:
			config_path = optarg;
			break;
		case '?':
			usage(stderr, argv[0]);
			return 2;
		default:
			if (given_count == (int)(sizeof(given) / sizeof(given[0]))) {
				fprintf(stderr, "too many options\n");
				return 2;
			}
			given[given_count].option = option;
			given[given_count++].arg = optarg;
			platform_option |= is_platform_option(option);
			break;
		}
	}
	if (optind < argc)
		application_path = argv[optind++];
	if (optind < argc) {
		if (config_path) {
			fprintf(stderr, "two configuration files\n");
			return 2;
		}
		config_path = argv[optind++];
	}
	if (optind < argc) {
		fprintf(stderr, "too many arguments: %s\n", argv[optind]);
		return 2;
	}

	/* the configuration, then the options over it */
	mp_ctx_init(&s.ctx, s.seed);
	if (config_path) {
		if (mp_read_configuration(config_path, &s.ctx.params, error,
					  sizeof(error))) {
			fprintf(stderr, "%s\n", error);
			return 2;
		}
		s.one_rule = true;
		s.algorithm = s.ctx.params.algorithm;
		if (platform_option && s.ctx.params.platform.count) {
			fprintf(stderr, "the --pcu-* and --zcu-* options are for "
				"the built-in zonal platform: set the cores of "
				"the platform of %s in the file\n", config_path);
			return 2;
		}
	}
	for (i = 0; i < given_count; i++)
		if (apply_option(&s, given[i].option, given[i].arg))
			return 2;
	if (!settings_valid(&s))
		return 2;
	if (s.print_config) {
		s.ctx.params.algorithm = s.one_rule ? s.algorithm :
					 s.ctx.params.algorithm;
		mp_write_configuration(stdout, &s.ctx.params);
		return 0;
	}

	/* the application */
	mp_rng_seed(&s.ctx.rng, s.seed);
	if (application_path) {
		if (s.detection || s.generate) {
			fprintf(stderr, "--detection-chain and --generate make an "
				"application: no application file with them\n");
			return 2;
		}
		if (mp_read_application(application_path, &s.ctx.params,
					&application, &names, error,
					sizeof(error))) {
			fprintf(stderr, "%s\n", error);
			return 2;
		}
		snprintf(origin, sizeof(origin), "%s", application_path);
	} else {
		if (s.detection ? mp_detection_chain_application(&s.ctx, &application) :
		    mp_generate_application(&s.ctx, &s.generator, &application)) {
			fprintf(stderr, "cannot generate the application\n");
			return 1;
		}
		if (s.detection)
			snprintf(origin, sizeof(origin),
				 "the detection chain");
		else
			snprintf(origin, sizeof(origin), "generated application "
				 "(seed %llu)", (unsigned long long)s.seed);
	}
	if (s.generate) {
		mp_write_application(stdout, &s.ctx.params, &application, NULL);
		mp_chain_array_free(&application);
		return 0;
	}
	print_input(&s.ctx, &application, origin);

	/* the rules: the one chosen, or the four */
	run_count = s.one_rule ? 1 : ALGORITHMS;
	for (a = 0; a < run_count; a++) {
		runs[a].algorithm = s.one_rule ? s.algorithm : all[a];
		s.ctx.params.algorithm = runs[a].algorithm;
		if (mp_run_design_point(&s.ctx, &application, NULL,
					&runs[a].point)) {
			fprintf(stderr, "error in the %s mapping\n",
				mp_algorithm_name(runs[a].algorithm));
			return 1;
		}
	}

	print_results(runs, run_count);
	print_sizing(&s.ctx.params, runs, run_count);
	if (s.show_cores)
		for (a = 0; a < run_count; a++) {
			if (!runs[a].point.feasible)
				continue;
			printf("\n  %s: cores in use\n",
			       mp_algorithm_name(runs[a].algorithm));
			print_core_table(&runs[a].point);
			printf("\n  %s: task-chains\n",
			       mp_algorithm_name(runs[a].algorithm));
			print_chain_table(&runs[a].point, &application, &names);
		}
	printf("\n");
	if (s.out) {
		if (write_outputs(s.out, runs, run_count, &application, &names))
			return 1;
		printf("  results written to %s/\n\n", s.out);
	}

	for (a = 0; a < run_count; a++)
		mp_design_point_free(&runs[a].point);
	mp_chain_array_free(&application);
	mp_names_free(&names);
	return 0;
}
