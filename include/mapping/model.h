/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MAPPING_MODEL_H
#define MAPPING_MODEL_H

#include <stdbool.h>
#include <stddef.h>

/*
 * System model (doc/mapping.pdf, Chapter 2).
 *
 * An application is a set of task-chains. Partitioning cuts a task-chain
 * into fragments, which are task-chains too: struct mp_chain represents
 * both, with fragment_index telling which piece of the original chain it
 * is. Fragments are assigned to the cores of the architecture; each core
 * also runs one LET task, stored as a chain of one task with is_let set.
 *
 * Units: times in microseconds, utilizations in parts per million of a
 * core (MP_UTILIZATION_UNIT, 800000 = 80 %), core speeds in percent of the
 * reference core.
 */

/*
 * ECUs (called controllers in the code). A platform has up to MP_MAX_ECUS
 * ECUs, numbered from 0 in the order of the configuration; sets of ECUs
 * are bit masks. The built-in zonal platform has a central computer (the
 * PCU) and five zone controllers, at the indices below.
 */
#define MP_MAX_ECUS		32
#define MP_CONTROLLER_ANY	MP_MAX_ECUS	/* free: any ECU */
#define MP_CONTROLLER_BIT(c)	(1u << (c))
#define MP_ALL_CONTROLLERS	0xffffffffu	/* every ECU of any platform */

/* the built-in zonal platform */
#define MP_ZONAL_ECUS		6
#define MP_CONTROLLER_PCU	0	/* central computer (8 cores) */
#define MP_CONTROLLER_ZCU1	1	/* zone controllers (4 cores each) */
#define MP_CONTROLLER_ZCU2	2
#define MP_CONTROLLER_ZCU3	3
#define MP_CONTROLLER_ZCU4	4
#define MP_CONTROLLER_ZCU5	5

/* role of an ECU in the E/E architecture */
enum mp_ecu_role {
	MP_ROLE_CENTRAL,	/* central (vehicle) computer: free task-chains
				 * go there first */
	MP_ROLE_DOMAIN,		/* domain controller */
	MP_ROLE_ZONE,		/* zone controller */
	MP_ROLE_ECU,		/* ECU of a distributed architecture */
	MP_ROLE_COUNT,
};

/*
 * types of cores: application cores of the central computer (big, and
 * little energy-efficient ones), real-time microcontroller cores of the
 * zone controllers, and lock-step safety cores
 */
#define MP_CORE_BIG		0
#define MP_CORE_LITTLE		1
#define MP_CORE_MCU		2
#define MP_CORE_LOCKSTEP	3
#define MP_CORE_TYPES		4
#define MP_ALL_TYPES		((1u << MP_CORE_TYPES) - 1)

#define MP_NAME_SIZE		32	/* names of ECUs, switches, chains */

/*
 * One ECU of the platform: its initial cores of each type, the speed of
 * each type it has (0: it has no core of that type), the type of the cores
 * the assignment adds, its memory for the tasks and its largest number of
 * cores.
 */
struct mp_ecu {
	char name[MP_NAME_SIZE];
	enum mp_ecu_role role;
	int cores[MP_CORE_TYPES];	/* initial cores of each type */
	int speed[MP_CORE_TYPES];	/* % of the reference core; 0: none */
	int added_type;			/* type of the cores added by the
					 * assignment */
	int memory;			/* KB for the tasks; 0: unlimited */
	int max_cores;			/* cores at most; 0: unlimited */
};

/* an explicit platform (count 0: the built-in zonal platform) */
struct mp_platform {
	int count;
	struct mp_ecu ecus[MP_MAX_ECUS];
};

/* utilization of a fully busy core: utilizations are in ppm */
#define MP_UTILIZATION_UNIT	1000000

/* default filling limit of a core, phi: 80 % */
#define MP_FILL_LIMIT		800000

/* speed of the reference core, percent (WCETs are given for it) */
#define MP_REFERENCE_SPEED	100

/* where a task is stored inside a core (indices, not owning) */
struct mp_task_position {
	int core_index;		/* index of the core in the core array */
	int chain_slot;		/* index of the chain in core->chains */
	int task_slot;		/* index of the task in chain->tasks */
};

/* one task (document: tau_{m,n}, with its WCET C and period T) */
struct mp_task {
	int wcet;		/* worst-case execution time C on the core
				 * that runs the task, microseconds */
	int reference_wcet;	/* C on the reference core (speed 100 %) */
	int period;		/* period T (implicit deadline), microseconds */
	int priority;		/* fixed priority, 1 = highest (the LET task) */
	int response_time;	/* worst-case response time R, microseconds */
	int utilization;	/* C / T, ppm, rounded up */
	int controller;		/* controller the task must run on (its
				 * sensor or actuator), or MP_CONTROLLER_ANY */
	unsigned allowed;	/* controllers it may run on (bit mask);
				 * 0: from @controller (one, or all) */
	bool colocate_next;	/* must share a core with the next task */
	unsigned types;		/* core types it may run on (bit mask);
				 * 0: all */
	short type_factor[MP_CORE_TYPES];
				/* WCET on a core of each type, % of the
				 * WCET at the same speed (0: 100) */
	int memory;		/* code and data, KB */
	int accesses;		/* requests to the shared memory of its
				 * controller per job, worst case in
				 * isolation (0: none) */
	int id_in_chain;	/* position in the original chain (topological
				 * order), also its precedence rank */
	int unique_id;		/* unique over the application */
	int chain_id;		/* id of the original task-chain */
	bool is_let;		/* the LET task of a core */
	int message_bytes;	/* size of the message the task produces */
	struct mp_task_position position;
};

struct mp_task_array {
	struct mp_task *items;
	int count;
	int capacity;
};

/* a task-chain or a fragment of one (document: xi_m, xi_{m,x}) */
struct mp_chain {
	int id;			/* id of the original task-chain */
	int fragment_index;	/* x: rank of the fragment in the chain */
	int utilization;	/* sum of the task utilizations, ppm */
	int period_gcd;		/* GCD of the task periods, microseconds */
	int controller;		/* controller the chain is bound to, or
				 * MP_CONTROLLER_ANY (free chain) */
	unsigned allowed;	/* controllers it may run on (bit mask);
				 * 0: from @controller (one, or all) */
	int replica_group;	/* chains of one group (> 0) are replicas:
				 * never on the same controller (or core);
				 * 0: none */
	int memory;		/* sum of the task memories, KB */
	int deadline;		/* end-to-end deadline of the original chain
				 * (maximum reaction time), us; 0 = none */
	bool is_let;		/* the LET task's own chain on a core */
	bool is_fragment;	/* produced by partitioning */
	bool is_assigned;	/* placed on a core */
	struct mp_task_array tasks;	/* in topological order */
};

struct mp_chain_array {
	struct mp_chain *items;
	int count;
	int capacity;
};

/* one processor core (document: P_{k,l}) */
struct mp_core {
	int id;
	int fill_limit;		/* phi, ppm */
	int let_wcet;		/* WCET of its LET task when it communicates */
	int speed;		/* percent of the reference core */
	int type;		/* MP_CORE_BIG ... MP_CORE_LOCKSTEP */
	bool schedulable;	/* every task meets its deadline */
	int load;		/* sum of the chain utilizations, LET included */
	int load_remaining;	/* fill_limit - load */
	int controller;		/* controller the core belongs to */
	int access_stall;	/* shared-resource contention: the time a
				 * memory request may wait for the other
				 * cores of the controller, ns (0: none) */
	bool is_empty;		/* no application task on the core */
	struct mp_chain_array chains;	/* LET chain first, then fragments */
	struct mp_task_array tasks;	/* all tasks of the core, flattened for
					 * the response-time analysis */
};

struct mp_core_array {
	struct mp_core *items;
	int count;
	int capacity;
};

/* arrays: append a copy (deep for chains and cores), copy, free */
int mp_task_array_push(struct mp_task_array *array, const struct mp_task *task);
int mp_task_array_copy(struct mp_task_array *copy,
		       const struct mp_task_array *original);
void mp_task_array_free(struct mp_task_array *array);

int mp_chain_copy(struct mp_chain *copy, const struct mp_chain *original);
void mp_chain_free(struct mp_chain *chain);
int mp_chain_array_push(struct mp_chain_array *array,
			const struct mp_chain *chain);
int mp_chain_array_copy(struct mp_chain_array *copy,
			const struct mp_chain_array *original);
void mp_chain_array_free(struct mp_chain_array *array);

int mp_core_copy(struct mp_core *copy, const struct mp_core *original);
void mp_core_free(struct mp_core *core);
int mp_core_array_push(struct mp_core_array *array, const struct mp_core *core);
int mp_core_array_copy(struct mp_core_array *copy,
		       const struct mp_core_array *original);
void mp_core_array_free(struct mp_core_array *array);

/* the controllers @task, or every task of @chain, may run on */
unsigned mp_task_allowed(const struct mp_task *task);
unsigned mp_chain_allowed(const struct mp_chain *chain);

/* sum of the task utilizations of @chain, stored in chain->utilization */
void mp_chain_update_utilization(struct mp_chain *chain);

/* GCD of the task periods (0 for no task), e.g. the LET period */
int mp_tasks_period_gcd(const struct mp_task_array *tasks);

/* C / T in ppm, rounded up */
int mp_utilization(int wcet, int period);

/* the WCET factor of @task on a core of type @type, % (100 if none) */
int mp_task_type_factor(const struct mp_task *task, int type);

/*
 * WCET of @task on a core of @speed percent and type @type (-1: none):
 * reference C x factor / speed, rounded up, at least 1 us
 */
int mp_task_wcet_on(const struct mp_task *task, int speed, int type);

/* utilization of @chain on a core of @speed percent and type @type, ppm */
int mp_chain_utilization_on(const struct mp_chain *chain, int speed,
			    int type);

/*
 * WCET of @task on @core, shared-resource contention included: the WCET
 * at the speed and type of the core, plus the wait of each of its memory
 * requests, accesses x core->access_stall (rounded up to the us)
 */
int mp_task_wcet_on_core(const struct mp_task *task,
			 const struct mp_core *core);

/* utilization of @chain on @core (contention included), ppm */
int mp_chain_utilization_on_core(const struct mp_chain *chain,
				 const struct mp_core *core);

/* convert the tasks of @chain to @core (WCET, utilization) */
void mp_chain_convert_to_core(struct mp_chain *chain,
			      const struct mp_core *core);

/* the core types @task, or every task of @chain, may run on */
unsigned mp_task_types(const struct mp_task *task);

/*
 * mp_chain_colocate() - give every group of co-located tasks of @chain
 * (consecutive tasks joined by colocate_next) the constraints of all its
 * tasks: the ECU of its bound tasks, the intersection of the allowed ECUs
 * and core types. Return: 0, or -EINVAL with *@task the first task of a
 * group whose constraints exclude each other.
 */
int mp_chain_colocate(struct mp_chain *chain, int *task);
unsigned mp_chain_types(const struct mp_chain *chain);

/*
 * Stable sorting. Every sort of the tool is stable: elements equal for the
 * comparator keep their order, so the result is unique and does not
 * depend on the platform's sort. @less returns
 * whether its first element must come before its second.
 */
int mp_stable_sort(void *items, int count, size_t element_size,
		   bool (*less)(const void *a, const void *b));

/* tasks by increasing priority value (highest priority first) */
int mp_sort_tasks_by_priority(struct mp_task_array *tasks);
/* tasks by increasing position in their chain (base priority assignment) */
int mp_sort_tasks_by_id_in_chain(struct mp_task_array *tasks);
/* tasks chain by chain (LET first, then chain id, position in chain) */
int mp_sort_tasks_by_chain(struct mp_task_array *tasks);
/* chains by decreasing utilization (bin packing order) */
int mp_sort_chains_by_decreasing_utilization(struct mp_chain_array *chains);
/* cores by increasing remaining room (fullest first), then id */
int mp_sort_cores_by_increasing_room(struct mp_core_array *cores);

#endif /* MAPPING_MODEL_H */
