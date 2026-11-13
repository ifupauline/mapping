/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MAPPING_EXPERIMENT_H
#define MAPPING_EXPERIMENT_H

#include <stdint.h>
#include <stdio.h>

#include "mapping/generator.h"
#include "mapping/lifecycle.h"
#include "mapping/pipeline.h"

/*
 * Experiment engine: many independent design points, run in parallel with
 * OpenMP. A design point is a point of the parameter space (struct
 * mp_point_spec) applied to one instance of the application. Instance i of
 * an experiment is generated from a seed derived from (experiment seed,
 * i) only, so every point sees the same applications, and the results do
 * not depend on the number of threads or on the order of execution.
 */

enum mp_application_kind {
	MP_APPLICATION_AUTOMOTIVE,	/* generated (default) */
	MP_APPLICATION_DETECTION,	/* the detection chain */
};

/* one point of the parameter space */
struct mp_point_spec {
	char label[40];			/* shown in tables and in the CSV */
	enum mp_application_kind application;
	struct mp_gen_params generator;	/* generated applications only */
	struct mp_params params;	/* methodology and architecture */
	struct mp_lifecycle_params lifecycle;	/* updates > 0: the mapping
						 * is the SOP of a platform,
						 * then software updates */
};

/* a point with the default parameters and the generator */
void mp_point_spec_default(struct mp_point_spec *spec);

/* the results of one design point, flattened for the CSV file */
struct mp_result_row {
	int point;			/* index in the point list */
	int instance;			/* instance of the application */
	uint64_t application_seed;	/* seed of that instance */
	struct mp_point_spec spec;
	int error;			/* negative errno if the run failed */

	int chain_count;		/* original task-chains */
	int task_count;
	bool feasible;
	int fragment_count;
	int cuts;
	int forced_cuts;
	int max_cuts;
	int core_count;			/* cores of the mapping, empty included */
	int architecture_cores;		/* cores of the initial architecture */
	struct mp_stats stats;
	struct mp_communication_summary communication;
	struct mp_phase_times times;
	struct mp_sizing sizing;
	struct mp_lifecycle_result lifecycle;	/* if the point has updates */
};

struct mp_experiment {
	const char *name;
	const struct mp_point_spec *points;
	int point_count;
	int instances;			/* applications per point */
	uint64_t seed;			/* experiment seed */
	int jobs;			/* threads, 0 = all processors */
	bool progress;			/* progress line on stderr */
};

/* name of an application kind, as in the CSV file */
const char *mp_application_name(enum mp_application_kind kind);

/* seed of instance @instance of an experiment of seed @seed */
uint64_t mp_instance_seed(uint64_t seed, int instance);

/*
 * mp_run_experiment() - run every point for every instance
 * @rows: set to an array of point_count x instances rows, instance-major
 *	  (free() it)
 *
 * Return: 0, or -ENOMEM. A failed design point is reported in its row.
 */
int mp_run_experiment(const struct mp_experiment *experiment,
		      struct mp_result_row **rows);

/* write @count rows as CSV with a header line (times last, prefixed t_) */
void mp_write_csv(FILE *out, const char *experiment,
		  const struct mp_result_row *rows, int count);

#endif /* MAPPING_EXPERIMENT_H */
