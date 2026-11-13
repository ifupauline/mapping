// SPDX-License-Identifier: Apache-2.0
/* Unit tests of the model: deep copies, period GCD, stable sorting. */
#include <errno.h>
#include <string.h>

#include "mapping/model.h"
#include "unit.h"

static struct mp_task task_with_period(int period, int id)
{
	struct mp_task task;

	memset(&task, 0, sizeof(task));
	task.period = period;
	task.id_in_chain = id;
	task.utilization = 10 * (id + 1);
	return task;
}

/* GCD of the task periods: the LET period of a core */
static void test_period_gcd(void)
{
	struct mp_task_array tasks = {0};
	struct mp_task task;

	CHECK_EQ(mp_tasks_period_gcd(&tasks), 0);
	task = task_with_period(50000, 0);
	mp_task_array_push(&tasks, &task);
	CHECK_EQ(mp_tasks_period_gcd(&tasks), 50000);
	task = task_with_period(25000, 1);
	mp_task_array_push(&tasks, &task);
	task = task_with_period(10000, 2);
	mp_task_array_push(&tasks, &task);
	CHECK_EQ(mp_tasks_period_gcd(&tasks), 5000);
	mp_task_array_free(&tasks);
}

/*
 * co-located tasks take the constraints of their group: a free task glued
 * to a driver is bound to its ECU; two drivers of different ECUs, or no
 * common core type, cannot share a core
 */
static void test_colocate(void)
{
	struct mp_chain chain;
	struct mp_task task;
	int bad = -1, i;

	memset(&chain, 0, sizeof(chain));
	for (i = 0; i < 3; i++) {
		task = task_with_period(10000, i);
		task.controller = MP_CONTROLLER_ANY;
		mp_task_array_push(&chain.tasks, &task);
	}
	chain.tasks.items[1].colocate_next = true;
	chain.tasks.items[2].controller = 3;
	CHECK_EQ(mp_chain_colocate(&chain, &bad), 0);
	CHECK_EQ(chain.tasks.items[0].controller, MP_CONTROLLER_ANY);
	CHECK_EQ(chain.tasks.items[1].controller, 3);
	CHECK_EQ(chain.tasks.items[2].controller, 3);

	chain.tasks.items[1].controller = 2;	/* another ECU */
	CHECK_EQ(mp_chain_colocate(&chain, &bad), -EINVAL);
	CHECK_EQ(bad, 1);

	chain.tasks.items[1].controller = 3;
	chain.tasks.items[1].types = 1u << MP_CORE_BIG;
	chain.tasks.items[2].types = 1u << MP_CORE_MCU;
	CHECK_EQ(mp_chain_colocate(&chain, &bad), -EINVAL);
	mp_task_array_free(&chain.tasks);
}

/* a copied core owns its own chains and tasks */
static void test_core_copy_is_deep(void)
{
	struct mp_core core, copy;
	struct mp_chain chain;
	struct mp_task task = task_with_period(1000, 0);

	memset(&core, 0, sizeof(core));
	memset(&chain, 0, sizeof(chain));
	mp_task_array_push(&chain.tasks, &task);
	mp_chain_array_push(&core.chains, &chain);
	mp_task_array_push(&core.tasks, &task);

	CHECK_EQ(mp_core_copy(&copy, &core), 0);
	copy.chains.items[0].tasks.items[0].period = 7;
	copy.tasks.items[0].period = 9;
	CHECK_EQ(core.chains.items[0].tasks.items[0].period, 1000);
	CHECK_EQ(core.tasks.items[0].period, 1000);
	CHECK(copy.chains.items[0].tasks.items != core.chains.items[0].tasks.items);

	mp_core_free(&copy);
	mp_core_free(&core);
	mp_chain_free(&chain);
}

/* chains of equal utilization keep their order (stable sort) */
static void test_sort_is_stable(void)
{
	struct mp_chain_array chains = {0};
	int sizes[] = {30, 50, 30, 50, 10, 30};
	int expected_order[] = {1, 3, 0, 2, 5, 4};	/* by original position */
	int i;

	for (i = 0; i < 6; i++) {
		struct mp_chain chain;

		memset(&chain, 0, sizeof(chain));
		chain.utilization = sizes[i];
		chain.period_gcd = i;	/* remembers the original position */
		mp_chain_array_push(&chains, &chain);
	}
	CHECK_EQ(mp_sort_chains_by_decreasing_utilization(&chains), 0);
	for (i = 0; i < 6; i++)
		CHECK_EQ(chains.items[i].period_gcd, expected_order[i]);
	mp_chain_array_free(&chains);
}

/* sorting by priority puts LET first on equal priority values */
static void test_priority_tie_break(void)
{
	struct mp_task_array tasks = {0};
	struct mp_task task = task_with_period(10, 3);

	task.priority = 2;
	mp_task_array_push(&tasks, &task);
	task.is_let = true;
	mp_task_array_push(&tasks, &task);
	CHECK_EQ(mp_sort_tasks_by_priority(&tasks), 0);
	CHECK(tasks.items[0].is_let);
	mp_task_array_free(&tasks);
}

int main(void)
{
	RUN_TEST(test_period_gcd);
	RUN_TEST(test_colocate);
	RUN_TEST(test_core_copy_is_deep);
	RUN_TEST(test_sort_is_stable);
	RUN_TEST(test_priority_tie_break);
	return unit_summary();
}
