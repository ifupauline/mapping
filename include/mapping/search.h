/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MAPPING_SEARCH_H
#define MAPPING_SEARCH_H

/*
 * Metaheuristic baselines for the assignment: simulated annealing and a
 * genetic algorithm. They solve the same problem as the heuristic of the
 * pipeline (the same sub-chains, from the same partitioning, the same
 * constraints: allowed ECUs and core types, memory, separation of the
 * replicas, utilization cap, schedulability under fixed priorities with
 * the optimal priority assignment), and minimize the same first
 * objective: the number of cores used. They do not run the deadline-driven
 * local search, and do not model shared-memory contention.
 *
 * Candidate cores: the cores of the architecture, then, for every ECU,
 * as many more cores of the type it adds as the sub-chains it may run
 * would fill (their utilization on that type over the cap, rounded up),
 * within its largest number of cores and at most 64.
 *
 * Energy of a mapping (both methods): M - 0.9 sum((1 - u_p / phi)^2) / M,
 * M the cores used and u_p their loads: fewer cores first, then the free
 * capacity gathered on few cores (the next ones to empty); a mapping with
 * less load costs less, so a sub-chain prefers a faster core.
 *
 * Simulated annealing (Kirkpatrick et al., 1983): from the first-fit
 * mapping of the sub-chains by decreasing utilization (each to the first
 * candidate core that takes it, the fastest cores first), @budget moves; a move relocates one sub-chain to another candidate core (70 %) or
 * exchanges two sub-chains of two cores (30 %); the moved sub-chain is,
 * half of the time, one of the less loaded of two cores in use, and the
 * target core is one in use with probability 0.8. A move that breaks a
 * constraint is not made; one that raises the energy by d is accepted
 * with probability exp(-d / T), the temperature T decreasing
 * geometrically from initial_temperature to final_temperature. Every
 * mapping visited is feasible and schedulable (a core that loses a
 * sub-chain stays schedulable: its response times can only decrease).
 *
 * Genetic algorithm (order-based, as for bin packing, Falkenauer 1996): an
 * individual is an order of the sub-chains, decoded by first fit with
 * every constraint checked, schedulability included: each sub-chain goes
 * to the first core in use that takes it, else opens the first candidate
 * core that does, the cores tried the fastest first (then in the order
 * they were opened, or in candidate order). Population
 * @population: the decreasing-utilization order and random orders; binary tournaments, order crossover (OX1, probability
 * 0.9), a swap mutation (probability 0.3), the two best kept;
 * @budget generations.
 *
 * Both are deterministic for a seed.
 */

#include <stdint.h>

#include "mapping/context.h"
#include "mapping/pipeline.h"

enum mp_search_method {
	MP_SEARCH_ANNEALING,
	MP_SEARCH_GENETIC,
};

struct mp_search_params {
	enum mp_search_method method;
	long budget;			/* moves (SA) or generations (GA) */
	uint64_t seed;
	double initial_temperature;	/* SA (0.03) */
	double final_temperature;	/* SA (0.0003) */
	int population;			/* GA (40) */
	bool local_search;		/* the deadline local search of the
					 * heuristic (step 6) on the best
					 * mapping found (false) */
};

/* the improvements of the best mapping found: CPU time and cores */
#define MP_SEARCH_TRACE 256

struct mp_search_trace {
	int count;
	double time[MP_SEARCH_TRACE];	/* CPU seconds since the start */
	int cores[MP_SEARCH_TRACE];
	long evaluations;		/* moves tried, or decodings */
};

void mp_search_params_default(struct mp_search_params *params,
			      enum mp_search_method method, long budget);

/*
 * mp_search() - map @application with a metaheuristic
 * @point: the result, as mp_run_design_point() fills it (the best mapping
 *	   found, priorities by the robust optimal assignment, paths and
 *	   statistics; point->feasible false if no feasible start exists)
 * @trace: the improvements over time (may be NULL)
 * Return: 0, -ENOMEM, or -ENOTSUP with shared-memory contention.
 */
int mp_search(const struct mp_ctx *base,
	      const struct mp_chain_array *application,
	      const struct mp_search_params *params,
	      struct mp_design_point *point, struct mp_search_trace *trace);

const char *mp_search_method_name(enum mp_search_method method);

#endif /* MAPPING_SEARCH_H */
