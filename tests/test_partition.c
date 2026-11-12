// SPDX-License-Identifier: Apache-2.0
/* Unit tests of partitioning. */
#include <string.h>

#include "mapping/partition.h"
#include "unit.h"

/* a chain of tasks with the given utilizations (permill, converted), T = 10 ms */
static struct mp_chain make_chain(const int *utilizations, int count)
{
	struct mp_chain chain;
	int i;

	memset(&chain, 0, sizeof(chain));
	for (i = 0; i < count; i++) {
		struct mp_task task;

		memset(&task, 0, sizeof(task));
		task.period = 10000;
		task.wcet = utilizations[i] * 10;	/* C = U * T */
		task.reference_wcet = task.wcet;
		task.utilization = mp_utilization(task.wcet, task.period);
		task.controller = MP_CONTROLLER_ANY;
		task.priority = i + 1;
		task.id_in_chain = i;
		mp_task_array_push(&chain.tasks, &task);
		chain.utilization += task.utilization;
	}
	return chain;
}

/* fragments of chain 0 must be contiguous and keep every task once */
static void check_fragments_cover_chain(const struct mp_chain_array *fragments,
					int task_count)
{
	int next_task = 0, index, i, j;

	for (index = 0; index < fragments->count; index++)
		for (i = 0; i < fragments->count; i++) {
			const struct mp_chain *fragment = &fragments->items[i];

			if (fragment->fragment_index != index)
				continue;
			for (j = 0; j < fragment->tasks.count; j++)
				CHECK_EQ(fragment->tasks.items[j].id_in_chain,
					 next_task++);
		}
	CHECK_EQ(next_task, task_count);
}

/* sigma = 0.4: 200+150 | 300 | 100+100+150; heavy task 450 alone */
static void test_cuts_at_size_limit(void)
{
	int utilizations[] = {200, 150, 300, 100, 100, 150, 450};
	struct mp_chain_array chains = {0};
	struct mp_chain chain = make_chain(utilizations, 7);
	struct mp_ctx ctx;
	int i;

	mp_ctx_init(&ctx, 1);
	ctx.params.sigma = 400000;	/* limit 39 % with the LET margin */
	mp_chain_array_push(&chains, &chain);
	mp_chain_free(&chain);

	CHECK_EQ(mp_partition(&ctx, &chains), 0);
	CHECK_EQ(chains.count, 4);
	CHECK_EQ(ctx.cuts, 3);
	CHECK_EQ(ctx.max_cuts, 6);
	for (i = 0; i < chains.count; i++) {
		/* multi-task fragments stay within the limit */
		if (chains.items[i].tasks.count > 1)
			CHECK(chains.items[i].utilization <= 390000);
		CHECK(chains.items[i].is_fragment);
	}
	/* sorted by decreasing utilization: the 450 task first */
	CHECK_EQ(chains.items[0].utilization, 450000);
	check_fragments_cover_chain(&chains, 7);
	mp_chain_array_free(&chains);
}

/* a chain that fits sigma is not cut */
static void test_small_chain_not_cut(void)
{
	int utilizations[] = {100, 200, 150};
	struct mp_chain_array chains = {0};
	struct mp_chain chain = make_chain(utilizations, 3);
	struct mp_ctx ctx;

	mp_ctx_init(&ctx, 1);
	ctx.params.sigma = 800000;
	mp_chain_array_push(&chains, &chain);
	mp_chain_free(&chain);

	CHECK_EQ(mp_partition(&ctx, &chains), 0);
	CHECK_EQ(chains.count, 1);
	CHECK_EQ(ctx.cuts, 0);
	CHECK_EQ(chains.items[0].utilization, 450000);
	mp_chain_array_free(&chains);
}

/*
 * sensor (ZCU2) > compute > compute > actuator (ZCU4): the tasks of a
 * fragment share their binding, so the chain is cut at both changes (2
 * forced cuts, 3 fragments), even with a large sigma
 */
static void test_cuts_at_binding_changes(void)
{
	int utilizations[] = {10, 100, 100, 10};
	struct mp_chain_array chains = {0};
	struct mp_chain chain = make_chain(utilizations, 4);
	struct mp_ctx ctx;
	int i, bound = 0;

	chain.tasks.items[0].controller = MP_CONTROLLER_ZCU2;
	chain.tasks.items[3].controller = MP_CONTROLLER_ZCU4;
	mp_ctx_init(&ctx, 1);
	ctx.params.sigma = 800000;
	mp_chain_array_push(&chains, &chain);
	mp_chain_free(&chain);

	CHECK_EQ(mp_partition(&ctx, &chains), 0);
	CHECK_EQ(chains.count, 3);
	CHECK_EQ(ctx.cuts, 2);
	CHECK_EQ(ctx.forced_cuts, 2);
	for (i = 0; i < chains.count; i++) {
		const struct mp_chain *fragment = &chains.items[i];
		int j;

		for (j = 0; j < fragment->tasks.count; j++)
			CHECK_EQ(fragment->tasks.items[j].controller,
				 fragment->controller);
		bound += fragment->controller != MP_CONTROLLER_ANY;
		if (fragment->fragment_index == 1)
			CHECK_EQ(fragment->tasks.count, 2);
	}
	CHECK_EQ(bound, 2);
	check_fragments_cover_chain(&chains, 4);
	mp_chain_array_free(&chains);
}

/*
 * Two patterns in one fragment: {C=40 ms, T=100 ms} then {C=3 ms,
 * T=5 ms}. In chain order the fast task would miss (R = 43 ms > 5 ms);
 * with rate monotonic priorities across patterns the fragment is
 * schedulable alone and stays whole.
 */
static void test_patterns_share_a_fragment(void)
{
	int utilizations[] = {400, 600};
	struct mp_chain_array chains = {0};
	struct mp_chain chain = make_chain(utilizations, 2);
	struct mp_ctx ctx;

	chain.tasks.items[0].period = 100000;
	chain.tasks.items[0].wcet = 40000;
	chain.tasks.items[1].period = 5000;
	chain.tasks.items[1].wcet = 3000;
	chain.tasks.items[0].utilization = 400000;
	chain.tasks.items[1].utilization = 600000;
	mp_chain_update_utilization(&chain);
	mp_ctx_init(&ctx, 1);
	ctx.params.sigma = 1000000 + MP_LET_MARGIN;
	mp_chain_array_push(&chains, &chain);
	mp_chain_free(&chain);

	CHECK_EQ(mp_partition(&ctx, &chains), 0);
	CHECK_EQ(chains.count, 1);
	CHECK_EQ(ctx.cuts, 0);
	mp_chain_array_free(&chains);
}

/*
 * Co-location: tasks 1 and 2 (20 % each) must share a core; with sigma
 * 0.3 (limit 29 %) they still form one fragment of 40 %, and the cut
 * comes after them.
 */
static void test_colocated_tasks_kept_together(void)
{
	int utilizations[] = {100, 200, 200, 100};
	struct mp_chain_array chains = {0};
	struct mp_chain chain = make_chain(utilizations, 4);
	struct mp_ctx ctx;
	int i, together = 0;

	chain.tasks.items[1].colocate_next = true;
	mp_ctx_init(&ctx, 1);
	ctx.params.sigma = 300000;
	mp_chain_array_push(&chains, &chain);
	mp_chain_free(&chain);

	CHECK_EQ(mp_partition(&ctx, &chains), 0);
	for (i = 0; i < chains.count; i++) {
		const struct mp_chain *fragment = &chains.items[i];
		int j;

		for (j = 0; j + 1 < fragment->tasks.count; j++)
			together += fragment->tasks.items[j].id_in_chain == 1 &&
				    fragment->tasks.items[j + 1].id_in_chain == 2;
	}
	CHECK_EQ(together, 1);
	check_fragments_cover_chain(&chains, 4);
	mp_chain_array_free(&chains);
}

/*
 * Allowed controllers: a free task that may only run on the zone
 * controllers is cut from the free tasks around it, and its fragment
 * keeps the mask and the memory of its tasks. Its limit is sigma of a
 * zone core: 39 % x 50 % = 19.5 % of the reference core.
 */
static void test_masks_and_memory(void)
{
	int utilizations[] = {100, 150, 150, 100};
	struct mp_chain_array chains = {0};
	struct mp_chain chain = make_chain(utilizations, 4);
	struct mp_ctx ctx;
	int i;

	for (i = 0; i < 4; i++)
		chain.tasks.items[i].memory = 100;
	chain.tasks.items[1].allowed = ZONAL_ZONES;
	chain.tasks.items[2].allowed = ZONAL_ZONES;
	mp_ctx_init(&ctx, 1);
	ctx.params.sigma = 400000;
	mp_chain_array_push(&chains, &chain);
	mp_chain_free(&chain);

	CHECK_EQ(mp_partition(&ctx, &chains), 0);
	/* {10} | {15} | {15} | {10}: 15 + 15 > 19.5 on a zone core */
	CHECK_EQ(chains.count, 4);
	CHECK_EQ(ctx.forced_cuts, 2);
	for (i = 0; i < chains.count; i++) {
		const struct mp_chain *fragment = &chains.items[i];
		int first = fragment->tasks.items[0].id_in_chain;

		CHECK_EQ(fragment->memory, 100 * fragment->tasks.count);
		CHECK_EQ(mp_chain_allowed(fragment),
			 first == 1 || first == 2 ? ZONAL_ZONES :
			 MP_ALL_CONTROLLERS);
	}
	mp_chain_array_free(&chains);
}

int main(void)
{
	RUN_TEST(test_cuts_at_size_limit);
	RUN_TEST(test_small_chain_not_cut);
	RUN_TEST(test_cuts_at_binding_changes);
	RUN_TEST(test_patterns_share_a_fragment);
	RUN_TEST(test_colocated_tasks_kept_together);
	RUN_TEST(test_masks_and_memory);
	return unit_summary();
}
