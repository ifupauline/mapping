// SPDX-License-Identifier: Apache-2.0
/* Unit tests of the response-time analysis and of the priority assignment. */
#include <errno.h>
#include <string.h>

#include "mapping/assign.h"
#include "mapping/let.h"
#include "mapping/sched.h"
#include "unit.h"

static struct mp_task make_task(int wcet, int period, int priority, int id)
{
	struct mp_task task;

	memset(&task, 0, sizeof(task));
	task.wcet = wcet;
	task.period = period;
	task.priority = priority;
	task.id_in_chain = id;
	return task;
}

/* response time of the task with @id after the analysis */
static int response_of(const struct mp_task_array *tasks, int id)
{
	int i;

	for (i = 0; i < tasks->count; i++)
		if (tasks->items[i].id_in_chain == id)
			return tasks->items[i].response_time;
	return -1;
}

/* C/T = 1/10, 2/20, 3/60 by priority give R = 1, 3, 6 */
static void test_textbook_example(void)
{
	struct mp_task_array tasks = {0};
	struct mp_task task;
	bool schedulable;

	task = make_task(1, 10, 1, 0);
	mp_task_array_push(&tasks, &task);
	task = make_task(2, 20, 2, 1);
	mp_task_array_push(&tasks, &task);
	task = make_task(3, 60, 3, 2);
	mp_task_array_push(&tasks, &task);

	CHECK_EQ(mp_response_times(&tasks, &schedulable), 0);
	CHECK(schedulable);
	CHECK_EQ(response_of(&tasks, 0), 1);
	CHECK_EQ(response_of(&tasks, 1), 3);
	CHECK_EQ(response_of(&tasks, 2), 6);
	mp_task_array_free(&tasks);
}

/*
 * {C=1, T=2} above {C=3, T=5}: the low-priority task is preempted 3 times,
 * R = 3 + 3 * 1 = 6 > 5. Counting each higher priority task once (the bug
 * of the original code) would give 4 and call it schedulable.
 */
static void test_multiple_preemptions(void)
{
	struct mp_task_array tasks = {0};
	struct mp_task task;
	bool schedulable;

	task = make_task(1, 2, 1, 0);
	mp_task_array_push(&tasks, &task);
	task = make_task(3, 5, 2, 1);
	mp_task_array_push(&tasks, &task);

	CHECK_EQ(mp_response_times(&tasks, &schedulable), 0);
	CHECK(!schedulable);
	CHECK(response_of(&tasks, 1) > 5);
	mp_task_array_free(&tasks);
}

/*
 * a fixed point needing several iterations: R = 15 + ceil(R/10) * 3 goes
 * 15 -> 21 -> 24 -> 24
 */
static void test_iterated_fixed_point(void)
{
	struct mp_task_array tasks = {0};
	struct mp_task task;
	bool schedulable;

	task = make_task(3, 10, 1, 0);
	mp_task_array_push(&tasks, &task);
	task = make_task(15, 100, 2, 1);
	mp_task_array_push(&tasks, &task);

	CHECK_EQ(mp_response_times(&tasks, &schedulable), 0);
	CHECK(schedulable);
	CHECK_EQ(response_of(&tasks, 1), 24);
	mp_task_array_free(&tasks);
}

/* the LET task of an empty core (C = T = 0) interferes with nobody */
static void test_empty_let_task_ignored(void)
{
	struct mp_task_array tasks = {0};
	struct mp_task task;
	bool schedulable;

	task = make_task(0, 0, 1, 0);
	task.is_let = true;
	mp_task_array_push(&tasks, &task);
	task = make_task(4, 10, 2, 1);
	mp_task_array_push(&tasks, &task);

	CHECK_EQ(mp_response_times(&tasks, &schedulable), 0);
	CHECK(schedulable);
	CHECK_EQ(response_of(&tasks, 1), 4);
	mp_task_array_free(&tasks);
}

/* a WCET above its period is rejected, tasks come back sorted */
static void test_invalid_task_and_sorting(void)
{
	struct mp_task_array tasks = {0};
	struct mp_task task;
	bool schedulable;

	task = make_task(2, 10, 3, 0);
	mp_task_array_push(&tasks, &task);
	task = make_task(1, 10, 1, 1);
	mp_task_array_push(&tasks, &task);
	CHECK_EQ(mp_response_times(&tasks, &schedulable), 0);
	CHECK_EQ(tasks.items[0].priority, 1);
	CHECK_EQ(tasks.items[1].priority, 3);

	task = make_task(20, 10, 2, 2);
	mp_task_array_push(&tasks, &task);
	CHECK_EQ(mp_response_times(&tasks, &schedulable), -EINVAL);
	mp_task_array_free(&tasks);
}

/*
 * A core (empty LET task) holding the chains given as {chain, C, T}: the
 * tasks of a chain in the given order.
 */
static void make_core(struct mp_ctx *ctx, struct mp_core_array *cores,
		      const int (*tasks)[3], int task_count)
{
	int i;

	mp_ctx_init(ctx, 1);
	memset(cores, 0, sizeof(*cores));
	mp_add_core(ctx, cores, MP_CONTROLLER_PCU);
	for (i = 0; i < task_count; i++) {
		struct mp_core *core = &cores->items[0];
		struct mp_chain *chain = NULL;
		struct mp_task task;
		int j;

		/* append to the chain of the same id, or create it */
		for (j = 0; j < core->chains.count; j++)
			if (!core->chains.items[j].is_let &&
			    core->chains.items[j].id == tasks[i][0])
				chain = &core->chains.items[j];
		if (!chain) {
			struct mp_chain empty;

			memset(&empty, 0, sizeof(empty));
			empty.id = tasks[i][0];
			mp_chain_array_push(&core->chains, &empty);
			chain = &core->chains.items[core->chains.count - 1];
		}
		task = make_task(tasks[i][1], tasks[i][2], 0,
				 chain->tasks.count);
		task.chain_id = chain->id;
		task.reference_wcet = tasks[i][1];
		task.utilization = mp_utilization(tasks[i][1], tasks[i][2]);
		mp_task_array_push(&chain->tasks, &task);
		mp_chain_update_utilization(chain);
	}
	mp_core_update_load(&cores->items[0]);
	mp_core_build_task_list(&cores->items[0], 0);
}

static const struct mp_task *chain_task(const struct mp_core *core,
					int chain_id, int id_in_chain)
{
	int i;

	for (i = 0; i < core->chains.count; i++)
		if (!core->chains.items[i].is_let &&
		    core->chains.items[i].id == chain_id)
			return &core->chains.items[i].tasks.items[id_in_chain];
	return NULL;
}

static int priority_of(const struct mp_core *core, int chain, int id)
{
	return chain_task(core, chain, id)->priority;
}

/*
 * chain 0 {C=5, T=100} and chain 1 {C=3, T=6}: in chain order, R = 3 + 5
 * = 8 > 6. Audsley puts chain 1 first: R = 3, and chain 0 R = 5 +
 * ceil(R / 6) 3 = 11 <= 100.
 */
static void test_audsley_fixes_core(void)
{
	static const int tasks[][3] = {{0, 5, 100}, {1, 3, 6}};
	struct mp_core_array cores;
	struct mp_ctx ctx;
	const struct mp_core *core;

	make_core(&ctx, &cores, tasks, 2);
	CHECK_EQ(mp_assign_priorities(&ctx, &cores.items[0]), 0);
	core = &cores.items[0];
	CHECK(core->schedulable);
	CHECK_EQ(ctx.priority_assignments, 1);
	CHECK_EQ(ctx.priority_successes, 1);
	CHECK_EQ(priority_of(core, 1, 0), 2);
	CHECK_EQ(chain_task(core, 1, 0)->response_time, 3);
	CHECK_EQ(priority_of(core, 0, 0), 3);
	CHECK_EQ(chain_task(core, 0, 0)->response_time, 11);
	CHECK_EQ(core->chains.items[0].tasks.items[0].priority, 1);	/* LET */
	mp_core_array_free(&cores);
}

/*
 * Two activation patterns of one chain, {C=5, T=100} then {C=3, T=6}:
 * they communicate through a register, so the second may run first.
 */
static void test_patterns_reordered(void)
{
	static const int tasks[][3] = {{0, 5, 100}, {0, 3, 6}};
	struct mp_core_array cores;
	struct mp_ctx ctx;
	const struct mp_core *core;

	make_core(&ctx, &cores, tasks, 2);
	CHECK_EQ(mp_assign_priorities(&ctx, &cores.items[0]), 0);
	core = &cores.items[0];
	CHECK(core->schedulable);
	CHECK(priority_of(core, 0, 1) < priority_of(core, 0, 0));
	CHECK_EQ(chain_task(core, 0, 0)->response_time, 11);
	mp_core_array_free(&cores);
}

/*
 * Tasks a, b of one pattern {C=1, T=10} and c {C=4, T=5} of another
 * chain: c must be on top (c below a, b misses: R = 4 + 2 > 5), and a
 * stays above b whatever the slack would prefer.
 */
static void test_pattern_precedence_kept(void)
{
	static const int tasks[][3] = {{0, 1, 10}, {0, 1, 10}, {1, 4, 5}};
	struct mp_core_array cores;
	struct mp_ctx ctx;
	const struct mp_core *core;

	make_core(&ctx, &cores, tasks, 3);
	CHECK_EQ(mp_assign_priorities(&ctx, &cores.items[0]), 0);
	core = &cores.items[0];
	CHECK(core->schedulable);
	CHECK_EQ(priority_of(core, 1, 0), 2);
	CHECK(priority_of(core, 0, 0) < priority_of(core, 0, 1));
	CHECK_EQ(chain_task(core, 0, 1)->response_time, 10);
	mp_core_array_free(&cores);
}

/* nothing fits the lowest level: unschedulable, precedence order kept */
static void test_unschedulable_core(void)
{
	static const int tasks[][3] = {{0, 3, 5}, {1, 3, 6}};
	struct mp_core_array cores;
	struct mp_ctx ctx;
	const struct mp_core *core;

	make_core(&ctx, &cores, tasks, 2);
	CHECK_EQ(mp_assign_priorities(&ctx, &cores.items[0]), 0);
	core = &cores.items[0];
	CHECK(!core->schedulable);
	CHECK_EQ(ctx.priority_successes, 0);
	CHECK_EQ(priority_of(core, 0, 0), 2);
	CHECK_EQ(priority_of(core, 1, 0), 3);
	mp_core_array_free(&cores);
}

/*
 * Robust choice: {C=1, T=10} and {C=1, T=100} fit in both orders; the
 * lowest level goes to the task left with the largest relative slack
 * (T = 100: 98 %, against 80 % for T = 10), which is rate monotonic here.
 */
static void test_robust_choice(void)
{
	static const int tasks[][3] = {{0, 1, 100}, {1, 1, 10}};
	struct mp_core_array cores;
	struct mp_ctx ctx;
	const struct mp_core *core;

	make_core(&ctx, &cores, tasks, 2);
	CHECK_EQ(mp_assign_priorities(&ctx, &cores.items[0]), 0);
	core = &cores.items[0];
	CHECK(core->schedulable);
	CHECK_EQ(priority_of(core, 1, 0), 2);
	CHECK_EQ(priority_of(core, 0, 0), 3);
	mp_core_array_free(&cores);
}

/* one task C = 200 of T = 1000: the WCET can grow 5 times */
static void test_scaling_factor_single_task(void)
{
	static const int tasks[][3] = {{0, 200, 1000}};
	struct mp_core_array cores;
	struct mp_ctx ctx;

	make_core(&ctx, &cores, tasks, 1);
	mp_assign_priorities(&ctx, &cores.items[0]);
	CHECK_NEAR(mp_core_scaling_factor(&cores.items[0]), 5.0, 0.01);
	mp_core_array_free(&cores);
}

/*
 * {C=1, T=2} above {C=1, T=10}, both WCETs x f: at f = 5/3 the second
 * task completes at R = f + 5f = 10 = T (5 jobs of the first task), and
 * any larger factor misses. Integer WCETs would allow no growth at all:
 * the analysis scales in nanoseconds.
 */
static void test_scaling_factor_integer_wcets(void)
{
	static const int tasks[][3] = {{0, 1, 2}, {1, 1, 10}};
	struct mp_core_array cores;
	struct mp_ctx ctx;

	make_core(&ctx, &cores, tasks, 2);
	mp_assign_priorities(&ctx, &cores.items[0]);
	CHECK(cores.items[0].schedulable);
	CHECK_NEAR(mp_core_scaling_factor(&cores.items[0]), 5.0 / 3, 0.005);
	mp_core_array_free(&cores);
}

/* priority assignment of the core built from @tasks */
static bool audsley_schedulable(const int (*tasks)[3], int count)
{
	struct mp_core_array cores;
	struct mp_ctx ctx;
	bool schedulable;

	make_core(&ctx, &cores, tasks, count);
	mp_assign_priorities(&ctx, &cores.items[0]);
	schedulable = cores.items[0].schedulable;
	mp_core_array_free(&cores);
	return schedulable;
}

/*
 * Any priority order respecting the patterns schedulable? Brute force
 * over the permutations: a task must come after the earlier tasks of its
 * chain with the same period.
 */
static bool some_order_schedulable(const int (*tasks)[3], int count)
{
	int order[8], i, j, k;
	bool found = false;

	for (i = 0; i < count; i++)
		order[i] = i;
	/* every permutation, in lexicographic order */
	for (;;) {
		struct mp_task_array set = {0};
		bool valid = true, schedulable;

		for (i = 0; i < count && valid; i++)
			for (j = i + 1; j < count; j++)
				if (tasks[order[i]][0] == tasks[order[j]][0] &&
				    tasks[order[i]][2] == tasks[order[j]][2] &&
				    order[i] > order[j])
					valid = false;
		if (valid) {
			for (i = 0; i < count; i++) {
				struct mp_task task = make_task(tasks[order[i]][1],
							tasks[order[i]][2],
							i + 2, i);

				mp_task_array_push(&set, &task);
			}
			mp_response_times(&set, &schedulable);
			found |= schedulable;
			mp_task_array_free(&set);
		}
		/* next permutation */
		for (i = count - 2; i >= 0 && order[i] > order[i + 1]; i--)
			;
		if (i < 0)
			break;
		for (k = count - 1; order[k] < order[i]; k--)
			;
		j = order[i];
		order[i] = order[k];
		order[k] = j;
		for (j = i + 1, k = count - 1; j < k; j++, k--) {
			int swap = order[j];

			order[j] = order[k];
			order[k] = swap;
		}
	}
	return found;
}

/*
 * On random small cores, Audsley finds a schedulable order exactly when
 * one exists (it is optimal), and the response times it reports are
 * those of its priorities.
 */
static void test_audsley_optimal(void)
{
	static const int periods[] = {4, 5, 10, 20, 25, 50};
	int trial, schedulable_sets = 0, chain_order_fails = 0;
	struct mp_rng rng;

	mp_rng_seed(&rng, 5);
	for (trial = 0; trial < 400; trial++) {
		int tasks[7][3], count = mp_rng_range(&rng, 2, 7), i;
		struct mp_task_array set = {0};
		bool audsley, in_order;

		for (i = 0; i < count; i++) {
			tasks[i][0] = mp_rng_range(&rng, 0, 2);	/* chain */
			tasks[i][2] = periods[mp_rng_range(&rng, 0, 5)];
			tasks[i][1] = mp_rng_range(&rng, 1, tasks[i][2] / 3);
		}
		audsley = audsley_schedulable(tasks, count);
		CHECK_EQ(audsley, some_order_schedulable(tasks, count));

		/* the precedence order itself (priorities 2, 3, ...) */
		for (i = 0; i < count; i++) {
			struct mp_task task = make_task(tasks[i][1],
							tasks[i][2], i + 2, i);

			mp_task_array_push(&set, &task);
		}
		mp_response_times(&set, &in_order);
		mp_task_array_free(&set);
		CHECK(!in_order || audsley);
		schedulable_sets += audsley;
		chain_order_fails += audsley && !in_order;
	}
	/* the random sets do exercise both outcomes */
	CHECK(schedulable_sets > 0 && schedulable_sets < 400);
	CHECK(chain_order_fails > 0);
}

int main(void)
{
	RUN_TEST(test_textbook_example);
	RUN_TEST(test_multiple_preemptions);
	RUN_TEST(test_iterated_fixed_point);
	RUN_TEST(test_empty_let_task_ignored);
	RUN_TEST(test_invalid_task_and_sorting);
	RUN_TEST(test_audsley_fixes_core);
	RUN_TEST(test_patterns_reordered);
	RUN_TEST(test_pattern_precedence_kept);
	RUN_TEST(test_unschedulable_core);
	RUN_TEST(test_robust_choice);
	RUN_TEST(test_scaling_factor_single_task);
	RUN_TEST(test_scaling_factor_integer_wcets);
	RUN_TEST(test_audsley_optimal);
	return unit_summary();
}
