// SPDX-License-Identifier: Apache-2.0
/* Unit tests of the placement phase: swapping and displacement. */
#include <errno.h>
#include <string.h>

#include "mapping/assign.h"
#include "mapping/let.h"
#include "mapping/placement.h"
#include "mapping/repair.h"
#include "mapping/sched.h"
#include "unit.h"

/* one fragment of one task: {chain id, fragment index, task id, C, T} */
struct fragment_spec {
	int chain_id, fragment_index, id_in_chain, wcet, period;
};

static void add_fragment(struct mp_core *core, const struct fragment_spec *spec)
{
	struct mp_chain fragment;
	struct mp_task task;
	int gcd;

	memset(&fragment, 0, sizeof(fragment));
	memset(&task, 0, sizeof(task));
	task.wcet = spec->wcet;
	task.period = spec->period;
	task.reference_wcet = spec->wcet;
	task.utilization = mp_utilization(spec->wcet, spec->period);
	task.controller = MP_CONTROLLER_ANY;
	task.id_in_chain = spec->id_in_chain;
	task.chain_id = spec->chain_id;
	mp_task_array_push(&fragment.tasks, &task);
	fragment.id = spec->chain_id;
	fragment.fragment_index = spec->fragment_index;
	fragment.controller = MP_CONTROLLER_ANY;
	fragment.is_fragment = true;
	mp_chain_update_utilization(&fragment);

	mp_core_load_with_chain(core, &fragment, &gcd);
	mp_core_add_chain(core, &fragment, gcd);
	mp_chain_free(&fragment);
}

/*
 * @count PCU cores, the fragments of @specs[k] going to core @where[k].
 * With @full, the cores may be filled up to 100 % and the LET task costs
 * nothing: the tests can then build cores that no priority order
 * schedules (non-harmonic periods above the Liu and Layland bound).
 */
static void make_cores(struct mp_ctx *ctx, struct mp_core_array *cores,
		       int count, const struct fragment_spec *specs,
		       const int *where, int spec_count, bool full)
{
	int i;

	mp_ctx_init(ctx, 1);
	ctx->params.chain_count = 10;
	ctx->next_chain_id = 10;
	if (full) {
		ctx->params.fill_limit = MP_UTILIZATION_UNIT;
		ctx->params.let_wcet = 0;
	}
	memset(cores, 0, sizeof(*cores));
	for (i = 0; i < count; i++)
		mp_add_core(ctx, cores, MP_CONTROLLER_PCU);
	for (i = 0; i < spec_count; i++)
		add_fragment(&cores->items[where[i]], &specs[i]);
	mp_schedule(ctx, cores);
}

/* whether the core with @core_id holds fragment (@chain_id, @index) */
static bool holds(const struct mp_core_array *cores, int core_id,
		  int chain_id, int index)
{
	const struct mp_core *core = &cores->items[mp_find_core(cores, core_id)];
	int i;

	for (i = 0; i < core->chains.count; i++)
		if (!core->chains.items[i].is_let &&
		    core->chains.items[i].id == chain_id &&
		    core->chains.items[i].fragment_index == index)
			return true;
	return false;
}

static bool all_schedulable(const struct mp_core_array *cores)
{
	int i;

	for (i = 0; i < cores->count; i++)
		if (!cores->items[i].schedulable)
			return false;
	return true;
}

/*
 * Core 0 holds the two patterns of chain 0: {C=1000, T=2000} and {C=2400,
 * T=5000}, 98 % of the core. Even rate monotonic misses: R = 2400 + 3 x
 * 1000 = 5400 > 5000. Core 1 is empty: displacement only tries it on its
 * second attempt, and moves one fragment there.
 */
static void test_displacement_to_empty_core(void)
{
	static const struct fragment_spec specs[] = {
		{0, 0, 0, 1000, 2000}, {0, 1, 1, 2400, 5000},
	};
	static const int where[] = {0, 0};
	struct mp_core_array cores;
	struct mp_ctx ctx;

	make_cores(&ctx, &cores, 2, specs, where, 2, true);
	CHECK(!cores.items[mp_find_core(&cores, 0)].schedulable);

	CHECK_EQ(mp_displacement(&ctx, &cores), 0);
	CHECK(all_schedulable(&cores));
	CHECK(holds(&cores, 0, 0, 0) != holds(&cores, 0, 0, 1));
	CHECK(!cores.items[mp_find_core(&cores, 1)].is_empty);
	mp_core_array_free(&cores);
}

/*
 * Cores 0 and 1 each hold such a chain and are both unschedulable.
 * Exchanging the first fragment of chain 0 with the second of chain 1
 * gathers the equal periods: 2 x {1000, 2000} (R = 2000) on one core,
 * 2 x {2400, 5000} (R = 4800) on the other.
 */
static void test_swapping_two_late_cores(void)
{
	static const struct fragment_spec specs[] = {
		{0, 0, 0, 1000, 2000}, {0, 1, 1, 2400, 5000},
		{1, 0, 0, 1000, 2000}, {1, 1, 1, 2400, 5000},
	};
	static const int where[] = {0, 0, 1, 1};
	struct mp_core_array cores;
	struct mp_ctx ctx;

	make_cores(&ctx, &cores, 2, specs, where, 4, true);
	CHECK(!cores.items[0].schedulable && !cores.items[1].schedulable);

	CHECK_EQ(mp_swapping(&ctx, &cores), 0);
	CHECK(all_schedulable(&cores));
	CHECK(holds(&cores, 0, 1, 1) && holds(&cores, 0, 0, 1));
	CHECK(holds(&cores, 1, 0, 0) && holds(&cores, 1, 1, 0));
	mp_core_array_free(&cores);
}

/* removing a fragment resizes the LET task; removing the last frees it */
static void test_remove_chain_updates_let(void)
{
	static const struct fragment_spec specs[] = {
		{0, 0, 0, 1000, 10000}, {1, 0, 0, 1000, 4000},
	};
	static const int where[] = {0, 0};
	struct mp_core_array cores;
	struct mp_ctx ctx;
	struct mp_core *core;

	make_cores(&ctx, &cores, 1, specs, where, 2, false);
	core = &cores.items[0];
	/* 10 % + 25 % + LET 50 / 2000 = 2.5 % */
	CHECK_EQ(core->load, 375000);
	CHECK_EQ(mp_core_remove_chain(core, 1, 0), 0);
	CHECK_EQ(core->load, 105000);	/* 10 % + 50 / 10000 */
	CHECK_EQ(mp_core_remove_chain(core, 1, 0), -ENOENT);
	CHECK_EQ(mp_core_remove_chain(core, 0, 0), 0);
	CHECK_EQ(core->load, 0);
	CHECK_EQ(core->chains.count, 1);
	mp_core_array_free(&cores);
}

/*
 * Chain 0 in two fragments of one pattern on two PCU cores, with a
 * deadline the hop makes it miss: end-to-end delay = 150 (WCRT below the LET
 * task) + 1050 (write) + 1000 (read) + 150 > 2000. The repair moves one
 * fragment so that the chain runs on one core (end-to-end delay 250).
 */
static void test_deadline_repair(void)
{
	static const struct fragment_spec specs[] = {
		{0, 0, 0, 100, 1000}, {0, 1, 1, 100, 1000},
	};
	static const int where[] = {0, 1};
	struct mp_core_array cores;
	struct mp_ctx ctx;
	int i, j;

	make_cores(&ctx, &cores, 2, specs, where, 2, false);
	for (i = 0; i < cores.count; i++)
		for (j = 0; j < cores.items[i].chains.count; j++)
			cores.items[i].chains.items[j].deadline = 2000;

	CHECK_EQ(mp_repair_deadlines(&ctx, &cores), 0);
	CHECK_EQ(ctx.deadline_repairs, 1);
	/* both fragments on the same core now, the other one empty */
	CHECK(holds(&cores, 0, 0, 0) == holds(&cores, 0, 0, 1));
	CHECK(cores.items[0].is_empty != cores.items[1].is_empty);
	CHECK(all_schedulable(&cores));
	mp_core_array_free(&cores);
}

/*
 * Consolidation: three cores at 30, 20 and 10 % (one fragment each, same
 * period). The lightest one is emptied into the fullest; then the second
 * lightest (20 %) goes to the first (now 40 %): one core is left at 60 %.
 * WFDU keeps its cores.
 */
static void test_consolidation(void)
{
	static const struct fragment_spec specs[] = {
		{0, 0, 0, 3000, 10000}, {1, 0, 0, 2000, 10000},
		{2, 0, 0, 1000, 10000},
	};
	static const int where[] = {0, 1, 2};
	struct mp_core_array cores;
	struct mp_ctx ctx;
	int emptied, used = 0, i;

	make_cores(&ctx, &cores, 3, specs, where, 3, false);
	ctx.params.algorithm = MP_WFDU;
	CHECK_EQ(mp_consolidation(&ctx, &cores, &emptied), 0);
	CHECK_EQ(emptied, 0);
	ctx.params.algorithm = MP_BFDU;
	CHECK_EQ(mp_consolidation(&ctx, &cores, &emptied), 0);
	CHECK_EQ(emptied, 2);
	for (i = 0; i < cores.count; i++)
		used += mp_core_period_gcd(&cores.items[i]) > 0;
	CHECK_EQ(used, 1);
	CHECK(holds(&cores, 0, 0, 0) && holds(&cores, 0, 1, 0) &&
	      holds(&cores, 0, 2, 0));
	mp_core_array_free(&cores);
}

int main(void)
{
	RUN_TEST(test_displacement_to_empty_core);
	RUN_TEST(test_swapping_two_late_cores);
	RUN_TEST(test_remove_chain_updates_let);
	RUN_TEST(test_deadline_repair);
	RUN_TEST(test_consolidation);
	return unit_summary();
}
