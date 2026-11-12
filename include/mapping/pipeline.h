/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MAPPING_PIPELINE_H
#define MAPPING_PIPELINE_H

#include "mapping/communication.h"
#include "mapping/context.h"
#include "mapping/model.h"
#include "mapping/stats.h"

/*
 * The mapping methodology on one design point: partitioning (sigma),
 * assignment (alpha), scheduling, placement (swapping, displacement,
 * deadline repair), then communication and metrics.
 */

/* CPU time of each phase, seconds (thread CPU time: exact per design
 * point even when several run in parallel) */
struct mp_phase_times {
	double partitioning;
	double assignment;
	double scheduling;
	double swapping;
	double displacement;
	double repair;		/* deadline repair */
	double communication;
	double total;
};

/* sizing for a margin (struct mp_params, target_scaling) */
struct mp_sizing {
	int fill_limit;		/* phi of the design returned, ppm */
	bool target_met;	/* feasible with the target scaling factor
				 * (true without a target) */
	int runs;		/* design points run to find phi */
	double cpu_time;	/* of all of them, s */
};

struct mp_design_point {
	/* inputs */
	int sigma;			/* partitioning size limit, ppm */
	enum mp_algorithm algorithm;	/* bin packing of the assignment */

	/* outputs */
	bool feasible;			/* every fragment found a core */
	struct mp_ctx ctx;		/* context at the end (counters) */
	struct mp_chain_array fragments;	/* partitioning output */
	struct mp_core_array cores;		/* final mapping */
	struct mp_path_array paths;		/* one per original chain */
	struct mp_communication_summary communication;
	struct mp_stats stats;
	struct mp_phase_times times;	/* of the design returned */
	struct mp_sizing sizing;
};

/* optional hook called after each phase of a design point */
struct mp_observer {
	/*
	 * @stage: "partitioned" (fragments ready), "assigned", "scheduled"
	 * or "placed" (cores ready)
	 */
	void (*phase_done)(void *data, const char *stage,
			   const struct mp_design_point *point);
	void *data;
};

/*
 * the task-chains as the methodology maps them: the memory requests of
 * the tasks without a count (contention), then the WCET margin
 */
void mp_prepare_chains(const struct mp_params *params,
		       struct mp_chain_array *chains);

/* CPU time of the calling thread, seconds */
double mp_thread_cpu_seconds(void);

/*
 * mp_run_design_point() - run the methodology on a copy of @application
 * @base: context to start from: its parameters (sigma, alpha, ...) define
 *	  the design point
 * @observer: may be NULL
 *
 * When the assignment is unfeasible, @point->feasible is false and only
 * the fragments are filled.
 *
 * With a target scaling factor (target_scaling), the design point is the
 * one of the largest phi, between sigma and the phi of @base, whose
 * mapping is feasible and reaches the target: phi of @base if it does,
 * else found by bisection (7 steps; a smaller phi leaves more room on
 * every core). If not even phi = sigma reaches it, the design point is
 * that one, with sizing.target_met false.
 *
 * Free @point with mp_design_point_free().
 * Return: 0 or a negative errno.
 */
int mp_run_design_point(const struct mp_ctx *base,
			const struct mp_chain_array *application,
			const struct mp_observer *observer,
			struct mp_design_point *point);

void mp_design_point_free(struct mp_design_point *point);

/* "BFDU", "WFDU", "FFDU" or "CADU" */
const char *mp_algorithm_name(enum mp_algorithm algorithm);

/* the algorithm named @name (any case); 0 or -EINVAL */
int mp_parse_algorithm(const char *name, enum mp_algorithm *algorithm);


#endif /* MAPPING_PIPELINE_H */
