/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MAPPING_SCHED_H
#define MAPPING_SCHED_H

#include <stdbool.h>

#include "mapping/context.h"
#include "mapping/model.h"

/*
 * Scheduling phase: worst-case response times under fixed-priority
 * preemptive scheduling, and priority assignment.
 */

/**
 * mp_response_times() - worst-case response time of every task
 * @tasks: the tasks of one core with their priorities; sorted by priority
 *	   on return, each with its response_time set
 * @schedulable: set to whether every task meets its implicit deadline
 *
 * R is the least fixed point of R = C + sum over higher priority tasks j of
 * ceil(R / T_j) * C_j. The iteration stops once R exceeds the deadline T,
 * so an unschedulable task gets a value above T, not its exact WCRT.
 * Return: 0, or -EINVAL if a task has a WCET above its period, -ENOMEM.
 */
int mp_response_times(struct mp_task_array *tasks, bool *schedulable);

/**
 * mp_core_build_task_list() - flatten the chains of @core into core->tasks
 * @core_index: index of the core in its array, stored in each task position
 *
 * Every task records where it comes from (chain and task slot) so results
 * can be written back. The list is sorted by position in the chain.
 * Return: 0 or -ENOMEM.
 */
int mp_core_build_task_list(struct mp_core *core, int core_index);

/* copy the priorities and response times of core->tasks into its chains */
void mp_core_write_back_tasks(struct mp_core *core);

/**
 * mp_assign_priorities() - priority assignment of one core
 *
 * Audsley's optimal priority assignment restricted to the orders that
 * respect precedence (a task above the later tasks of its activation
 * pattern on the core); the LET task keeps priority 1. If a schedulable
 * order exists, it is found; among the candidates of each level, the one
 * left with the largest relative slack is chosen (robust choice).
 * core->tasks must have been built; the results are written back to the
 * chains and core->schedulable is set.
 * Return: 0 or a negative errno.
 */
int mp_assign_priorities(struct mp_ctx *ctx, struct mp_core *core);

/*
 * mp_assign_priorities_first_fit() - the same verdict, faster: each level
 * goes to the first candidate that fits (plain Audsley). The priorities,
 * and so the response times, may differ from the robust ones; used to
 * screen trial moves.
 */
int mp_assign_priorities_first_fit(struct mp_ctx *ctx, struct mp_core *core);

/**
 * mp_schedule() - scheduling phase of a design point
 *
 * Sorts @cores by increasing remaining room (fullest core first; later
 * phases rely on this order), builds every task
 * list and assigns priorities on every loaded core. Records the
 * schedulable rate in ctx->rate_after_scheduling.
 * Return: 0 or a negative errno.
 */
int mp_schedule(struct mp_ctx *ctx, struct mp_core_array *cores);

/**
 * mp_core_scaling_factor() - critical WCET scaling factor of a core
 *
 * The largest factor by which every application WCET of @core can be
 * multiplied (the LET task is unchanged) with its tasks still meeting
 * their deadlines under the current priorities, to within 0.1 %; below 1
 * when the core is unschedulable as it is. Searched by bisection, at most
 * MP_MAX_SCALING_FACTOR. Return: the factor, or -1 for a core without
 * application tasks.
 */
double mp_core_scaling_factor(const struct mp_core *core);

#define MP_MAX_SCALING_FACTOR 64.0

/*
 * mp_count_schedulable_cores() - count the schedulable and unschedulable
 * cores of @cores into @ctx and set ctx->schedulable_rate (0 to 1).
 */
void mp_count_schedulable_cores(struct mp_ctx *ctx,
				const struct mp_core_array *cores);

#endif /* MAPPING_SCHED_H */
