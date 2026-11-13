/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MAPPING_CONTEXT_H
#define MAPPING_CONTEXT_H

#include <stdbool.h>
#include <stddef.h>

#include "mapping/model.h"
#include "mapping/network.h"
#include "mapping/rng.h"

/* bin-packing algorithm of the assignment step (alpha) */
enum mp_algorithm {
	MP_BFDU = 1,	/* best fit, decreasing utilization */
	MP_WFDU = 2,	/* worst fit, decreasing utilization */
	MP_FFDU = 3,	/* first fit, decreasing utilization */
	MP_CADU = 4,	/* communication aware: fewest hops created, then
			 * the PCU, then best fit */
};

/* how far apart the replicas of a chain must run */
enum mp_separation {
	MP_SEPARATE_CONTROLLERS,	/* never on the same controller */
	MP_SEPARATE_CORES,		/* never on the same core */
};

/* input parameters of one design point (defaults: mp_params_default) */
struct mp_params {
	enum mp_algorithm algorithm;	/* alpha (FFDU) */
	int sigma;			/* partitioning size limit, ppm (400000) */
	int chain_count;		/* chains of the application, set by the
					 * generator */

	/*
	 * platform: an explicit list of ECUs (platform.count > 0, from a
	 * configuration file), else the built-in zonal platform made of the
	 * PCU and five zone controllers described by the fields below
	 */
	struct mp_platform platform;
	int pcu_cores;			/* cores of the central computer (8) */
	int zcu_cores;			/* cores of each zone controller (4) */
	int pcu_little_cores;		/* little cores of the PCU, after the
					 * big ones (0) */
	int zcu_lockstep_cores;		/* lock-step cores of each zone
					 * controller, after the others (0) */
	int little_speed;		/* speed of a little core, % of the
					 * reference core (40) */
	int pcu_speed;			/* speed of a PCU core, % of the
					 * reference core (100) */
	int zcu_speed;			/* speed of a ZCU core, % (50) */
	int fill_limit;			/* phi, ppm (800000) */
	int let_wcet;			/* WCET of the LET tasks on the
					 * reference core, us (50) */
	int pcu_memory;			/* memory of the PCU for the tasks, KB;
					 * 0: unlimited (0) */
	int zcu_memory;			/* memory of each zone controller, KB;
					 * 0: unlimited (0) */
	struct mp_net_config network;	/* TSN network */
	int memory_latency;		/* shared-resource contention: time to
					 * serve one request at the shared
					 * memory of a controller, ns; the
					 * requests of its cores are served in
					 * round robin (0: no contention) */
	int access_density;		/* requests per ms of reference WCET
					 * of the tasks that do not give their
					 * own count, the LET tasks included
					 * (1000) */
	int competing_cores[MP_MAX_ECUS];
					/* contention, set by the pipeline:
					 * cores of each controller that may
					 * compete for its memory when more
					 * than in the architecture (0) */

	/* methodology options */
	int wcet_margin;		/* every WCET is increased by this
					 * percentage before the mapping (0) */
	bool affinity;			/* assignment prefers the cores holding
					 * the neighbouring fragments of a chain
					 * (true) */
	bool repair_deadlines;		/* placement moves fragments to meet
					 * the chain deadlines (true) */
	enum mp_separation separation;	/* replicas on different controllers,
					 * or only on different cores
					 * (MP_SEPARATE_CONTROLLERS) */
	bool central_only;		/* free fragments only on the central
					 * computers, when the platform has
					 * one (false; a practice baseline) */
	bool swap_moves;		/* placement: also swap moves between
					 * unschedulable cores (false: in the
					 * experiments they never repaired a
					 * core) */
	bool relocation_moves;		/* placement: relocation moves from
					 * unschedulable cores (true) */
	bool consolidate;		/* after the assignment, empty the
					 * cores whose fragments fit elsewhere
					 * (not with WFDU) (true) */
	bool fixed_hardware;		/* the assignment adds no core: a
					 * fragment that fits nowhere makes the
					 * mapping infeasible (false; software
					 * updates of a platform) */
	int target_scaling;		/* sizing for a margin: the WCET
					 * scaling factor the mapping must
					 * reach, permille; phi is lowered
					 * until it does (0: no sizing) */
};

/*
 * The ECUs of the platform of @params: the explicit platform, or the
 * built-in zonal one. @ecu is an index from 0 to mp_ecu_count() - 1.
 */
int mp_ecu_count(const struct mp_params *params);
void mp_ecu_get(const struct mp_params *params, int ecu, struct mp_ecu *out);
const char *mp_ecu_name(const struct mp_params *params, int ecu);
enum mp_ecu_role mp_ecu_role(const struct mp_params *params, int ecu);
/* speed of a core of type @type on @ecu, % (0: @ecu has no such core) */
int mp_ecu_speed(const struct mp_params *params, int ecu, int type);
/* the core types of @ecu (bit mask): those with a speed */
unsigned mp_ecu_types(const struct mp_params *params, int ecu);
/* memory of @ecu for the tasks, KB (0: unlimited) */
int mp_ecu_memory(const struct mp_params *params, int ecu);
/* initial cores of @ecu, all types */
int mp_ecu_initial_cores(const struct mp_params *params, int ecu);
/* largest number of cores of @ecu (0: unlimited) */
int mp_ecu_max_cores(const struct mp_params *params, int ecu);
bool mp_ecu_is_central(const struct mp_params *params, int ecu);
/*
 * the ECUs that host the free sub-chains first: the central computers,
 * or, on a platform without one, the ECUs with the fastest initial cores
 */
bool mp_ecu_is_host(const struct mp_params *params, int ecu);
/* whether a chain allowed on @allowed with core types @types may run on a
 * host */
bool mp_host_allowed(const struct mp_params *params, unsigned allowed,
		     unsigned types);
/* the index of the ECU called @name, or -1 */
int mp_find_ecu(const struct mp_params *params, const char *name);
/*
 * the ECUs in the order the assignment prefers for a new core: the
 * hosts, then the others, each group by index; returns
 * their number
 */
int mp_ecu_preference(const struct mp_params *params, int *order);
/* the ECUs that sensors and actuators are wired to: all but the central
 * computers (all if every ECU is central) */
unsigned mp_io_ecus(const struct mp_params *params);
/* whether a chain allowed on @allowed with core types @types may run on a
 * central computer */
bool mp_central_allowed(const struct mp_params *params, unsigned allowed,
			unsigned types);
/* the built-in zonal platform of @params as an explicit one */
void mp_platform_zonal(const struct mp_params *params,
		       struct mp_platform *platform);
/* -EINVAL with a message in @error if the platform is inconsistent */
int mp_platform_check(const struct mp_params *params, char *error,
		      size_t size);

/*
 * the wait of a memory request on a core of a controller with @cores
 * cores, ns: the memory latency times the other cores (round robin)
 */
int mp_access_stall(const struct mp_params *params, int cores);

/*
 * the cores of @controller that may compete for its memory during the
 * assignment: those of the architecture, or competing_cores if more
 */
int mp_competing_cores(const struct mp_params *params, int controller);

/* requests of a task of reference WCET @wcet without its own count */
int mp_default_accesses(const struct mp_params *params, int wcet);

/*
 * Everything one design point needs besides its chains and cores. There is
 * no global state: two contexts can be used by two threads at once.
 */
struct mp_ctx {
	struct mp_rng rng;		/* random draws of this design point */
	struct mp_params params;
	int cuts;			/* cuts made by partitioning */
	int max_cuts;			/* tasks - 1 per chain, summed */
	int forced_cuts;		/* cuts between two controllers */

	/* architecture and assignment */
	int core_count;			/* cores created so far (next core id) */
	int architecture_cores;		/* cores of the initial architecture */
	int next_chain_id;		/* id of the next LET chain */
	bool feasible;			/* every fragment could be assigned */
	int unplaced_chain;		/* if not: the chain and the index of */
	int unplaced_fragment;		/* the first fragment that fits nowhere */
	int reserved_memory[MP_MAX_ECUS];	/* during the
					 * assignment: memory (KB) of the bound
					 * fragments not placed yet, which free
					 * fragments may not take */

	/* priority assignment, counted over the design point */
	int priority_assignments;	/* cores whose priorities were set */
	int priority_successes;		/* of which schedulable */

	/* schedulable cores (last count) and rate after each phase */
	int schedulable_cores;
	int unschedulable_cores;
	float schedulable_rate;		/* schedulable / all cores, last count */
	float rate_after_scheduling;	/* SR after priority assignment */
	float rate_after_swapping;	/* SR after swapping */
	float rate_after_displacement;	/* SR after displacement */
	float rate_final;		/* SR after the deadline repair */
	int deadline_repairs;		/* moves kept by the deadline repair */
	int emptied_cores;		/* cores emptied by the consolidation */
};

/* the default parameters (documented in struct mp_params) */
void mp_params_default(struct mp_params *params);

/* a context with default parameters and its generator seeded with @seed */
void mp_ctx_init(struct mp_ctx *ctx, uint64_t seed);

#endif /* MAPPING_CONTEXT_H */
