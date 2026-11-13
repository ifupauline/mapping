// SPDX-License-Identifier: Apache-2.0
/* Unit tests of the LET task sizing, core speeds and bin-packing assignment. */
#include <errno.h>
#include <string.h>

#include "mapping/assign.h"
#include "mapping/let.h"
#include "unit.h"

/*
 * a one-task chain of utilization @utilization (ppm of the reference core)
 * and period @period
 */
static struct mp_chain make_chain(int id, int utilization, int period,
				  int controller)
{
	struct mp_chain chain;
	struct mp_task task;

	memset(&chain, 0, sizeof(chain));
	memset(&task, 0, sizeof(task));
	task.period = period;
	task.wcet = (int)((int64_t)utilization * period / MP_UTILIZATION_UNIT);
	task.reference_wcet = task.wcet;
	task.utilization = mp_utilization(task.wcet, period);
	task.priority = 1;
	task.chain_id = id;
	task.controller = controller;
	mp_task_array_push(&chain.tasks, &task);
	chain.id = id;
	chain.utilization = task.utilization;
	chain.period_gcd = period;
	chain.controller = controller;
	return chain;
}

/* a context with @core_count PCU cores and the given algorithm */
static void make_cores(struct mp_ctx *ctx, struct mp_core_array *cores,
		       int core_count, enum mp_algorithm algorithm)
{
	int i;

	mp_ctx_init(ctx, 1);
	ctx->params.algorithm = algorithm;
	ctx->params.chain_count = 10;
	ctx->next_chain_id = 10;
	memset(cores, 0, sizeof(*cores));
	for (i = 0; i < core_count; i++)
		mp_add_core(ctx, cores, MP_CONTROLLER_PCU);
}

/* C = 50 us over the GCD, rounded up; nothing for an empty core */
static void test_let_utilization(void)
{
	CHECK_EQ(MP_LET_WCET, 50);
	CHECK_EQ(mp_let_utilization(MP_LET_WCET, 0), 0);
	CHECK_EQ(mp_let_utilization(MP_LET_WCET, 1000), 50000);
	CHECK_EQ(mp_let_utilization(MP_LET_WCET, 5000), 10000);
	CHECK_EQ(mp_let_utilization(MP_LET_WCET, 3000), 16667);	/* up */
	CHECK_EQ(mp_let_utilization(200, 1000), 200000);
}

static void test_let_update_and_load(void)
{
	struct mp_core_array cores;
	struct mp_ctx ctx;
	struct mp_core *core;
	const struct mp_task *let_task;

	make_cores(&ctx, &cores, 1, MP_FFDU);
	core = &cores.items[0];
	CHECK_EQ(mp_let_chain_index(core), 0);
	CHECK_EQ(core->chains.items[0].id, 10);
	CHECK_EQ(core->let_wcet, MP_LET_WCET);
	CHECK_EQ(mp_core_update_load(core), 0);

	CHECK_EQ(mp_let_update(core, 2000), 0);
	let_task = &core->chains.items[0].tasks.items[0];
	CHECK_EQ(let_task->wcet, MP_LET_WCET);
	CHECK_EQ(let_task->period, 2000);
	CHECK_EQ(let_task->priority, 1);
	CHECK_EQ(mp_core_update_load(core), 25000);
	CHECK_EQ(core->load_remaining, MP_FILL_LIMIT - 25000);

	CHECK_EQ(mp_let_update(core, 0), 0);
	CHECK_EQ(let_task->wcet, 0);
	CHECK_EQ(mp_core_update_load(core), 0);
	mp_core_array_free(&cores);
}

/* the LET task is resized for the GCD of old and new periods */
static void test_load_with_chain(void)
{
	struct mp_core_array cores;
	struct mp_chain first = make_chain(0, 300000, 10000, MP_CONTROLLER_ANY);
	struct mp_chain second = make_chain(1, 100000, 4000, MP_CONTROLLER_ANY);
	struct mp_ctx ctx;
	int gcd;

	make_cores(&ctx, &cores, 1, MP_FFDU);
	/* 30 % + LET 50 / 10000 = 0.5 % */
	CHECK_EQ(mp_core_load_with_chain(&cores.items[0], &first, &gcd), 305000);
	CHECK_EQ(gcd, 10000);

	CHECK_EQ(mp_core_add_chain(&cores.items[0], &first, gcd), 0);
	CHECK_EQ(cores.items[0].load, 305000);
	/* 30 % + 10 % + LET 50 / gcd(10000, 4000) = 50 / 2000 = 2.5 % */
	CHECK_EQ(mp_core_load_with_chain(&cores.items[0], &second, &gcd), 425000);
	CHECK_EQ(gcd, 2000);

	mp_chain_free(&first);
	mp_chain_free(&second);
	mp_core_array_free(&cores);
}

/*
 * Core speeds: WCETs are given on the reference core; a core at half speed
 * doubles them (rounded up), its LET copy too.
 */
static void test_core_speed(void)
{
	struct mp_core_array cores = {0};
	struct mp_chain chain = make_chain(0, 100000, 10000, MP_CONTROLLER_ANY);
	const struct mp_chain *placed;
	struct mp_core *zcu;
	struct mp_ctx ctx;
	int gcd;

	CHECK_EQ(mp_task_wcet_on(&chain.tasks.items[0], 100, -1), 1000);
	CHECK_EQ(mp_task_wcet_on(&chain.tasks.items[0], 50, -1), 2000);
	CHECK_EQ(mp_task_wcet_on(&chain.tasks.items[0], 75, -1), 1334);	/* up */
	CHECK_EQ(mp_chain_utilization_on(&chain, 50, -1), 200000);

	mp_ctx_init(&ctx, 1);
	ctx.params.chain_count = 1;
	ctx.next_chain_id = 1;
	CHECK_EQ(mp_add_core(&ctx, &cores, MP_CONTROLLER_ZCU1), 0);
	zcu = &cores.items[0];
	CHECK_EQ(zcu->speed, 50);
	CHECK_EQ(zcu->let_wcet, 100);
	/* 20 % + LET 100 / 10000 = 1 % */
	CHECK_EQ(mp_core_load_with_chain(zcu, &chain, &gcd), 210000);
	CHECK_EQ(mp_core_add_chain(zcu, &chain, gcd), 0);
	placed = &zcu->chains.items[1];
	CHECK_EQ(placed->tasks.items[0].wcet, 2000);
	CHECK_EQ(placed->tasks.items[0].reference_wcet, 1000);
	CHECK_EQ(placed->utilization, 200000);
	CHECK_EQ(zcu->load, 210000);
	mp_chain_free(&chain);
	mp_core_array_free(&cores);
}

/*
 * Two cores holding 50 % and 20 %: a 10 % chain goes to the fullest core
 * with BFDU, to the emptiest with WFDU and to the first with FFDU.
 */
static int core_receiving(enum mp_algorithm algorithm)
{
	struct mp_chain_array chains = {0};
	struct mp_core_array cores;
	struct mp_chain chain;
	struct mp_ctx ctx;
	int receiver = -1, i;

	make_cores(&ctx, &cores, 2, algorithm);
	chain = make_chain(0, 500000, 10000, MP_CONTROLLER_PCU);
	mp_core_add_chain(&cores.items[1], &chain, 10000);
	mp_chain_free(&chain);
	chain = make_chain(1, 200000, 10000, MP_CONTROLLER_PCU);
	mp_core_add_chain(&cores.items[0], &chain, 10000);
	mp_chain_free(&chain);

	chain = make_chain(2, 100000, 10000, MP_CONTROLLER_PCU);
	mp_chain_array_push(&chains, &chain);
	mp_chain_free(&chain);
	CHECK_EQ(mp_assign(&ctx, &chains, &cores), 0);
	CHECK(ctx.feasible);
	CHECK_EQ(cores.count, 2);
	for (i = 0; i < cores.count; i++)
		if (cores.items[i].chains.count == 3)
			receiver = cores.items[i].id;
	mp_chain_array_free(&chains);
	mp_core_array_free(&cores);
	return receiver;
}

static void test_bin_packing_choices(void)
{
	CHECK_EQ(core_receiving(MP_BFDU), 1);
	CHECK_EQ(core_receiving(MP_WFDU), 0);
	CHECK_EQ(core_receiving(MP_FFDU), 0);
}

/* a bound chain only goes to its controller; a new core is added there */
static void test_controller_respected_and_core_added(void)
{
	struct mp_chain_array chains = {0};
	struct mp_core_array cores = {0};
	/* 70 % on a ZCU core at half speed */
	struct mp_chain chain = make_chain(0, 350000, 10000, MP_CONTROLLER_ZCU3);
	struct mp_ctx ctx;
	int i, zcu3_cores = 0;

	mp_ctx_init(&ctx, 1);
	ctx.params.algorithm = MP_BFDU;
	ctx.params.chain_count = 5;
	CHECK_EQ(mp_architecture_init(&ctx, &cores), 0);
	CHECK_EQ(cores.count, ctx.params.pcu_cores + 5 * ctx.params.zcu_cores);
	CHECK_EQ(ctx.architecture_cores, 28);
	/* LET chains numbered after the application */
	CHECK_EQ(cores.items[1].chains.items[0].id, 6);

	/* 5 chains on ZCU3: its 4 cores, then a fifth one */
	for (i = 0; i < 5; i++) {
		chain.id = i;
		mp_chain_array_push(&chains, &chain);
	}
	CHECK_EQ(mp_assign(&ctx, &chains, &cores), 0);
	CHECK(ctx.feasible);
	CHECK_EQ(cores.count, 29);
	CHECK_EQ(cores.items[28].controller, MP_CONTROLLER_ZCU3);
	CHECK_EQ(cores.items[28].speed, 50);
	for (i = 0; i < cores.count; i++) {
		if (cores.items[i].controller == MP_CONTROLLER_ZCU3)
			zcu3_cores++;
		else
			CHECK_EQ(cores.items[i].chains.count, 1);
		CHECK(cores.items[i].load <= cores.items[i].fill_limit);
	}
	CHECK_EQ(zcu3_cores, 5);
	mp_chain_free(&chain);
	mp_chain_array_free(&chains);
	mp_core_array_free(&cores);
}

/* the controller receiving the free fragment @chain_id */
static int controller_of(const struct mp_core_array *cores, int chain_id,
			 int fragment_index)
{
	int i, j;

	for (i = 0; i < cores->count; i++)
		for (j = 0; j < cores->items[i].chains.count; j++)
			if (!cores->items[i].chains.items[j].is_let &&
			    cores->items[i].chains.items[j].id == chain_id &&
			    cores->items[i].chains.items[j].fragment_index ==
			    fragment_index)
				return cores->items[i].controller;
	return -2;
}

/*
 * Free fragments go to the central computer first, even with WFDU (the
 * empty zone cores have as much room): the ZCU cores take them only when
 * no PCU core can.
 */
static void test_free_fragments_prefer_pcu(void)
{
	struct mp_chain_array chains = {0};
	struct mp_core_array cores = {0};
	struct mp_ctx ctx;
	int i, on_pcu = 0, on_zcus = 0;

	mp_ctx_init(&ctx, 1);
	ctx.params.algorithm = MP_WFDU;
	ctx.params.chain_count = 12;
	CHECK_EQ(mp_architecture_init(&ctx, &cores), 0);
	/* 12 x 70 %: one per PCU core (8), then 4 more */
	for (i = 0; i < 12; i++) {
		struct mp_chain chain = make_chain(i, 300000, 10000,
						   MP_CONTROLLER_ANY);

		mp_chain_array_push(&chains, &chain);
		mp_chain_free(&chain);
		chains.items[i].utilization = 700000;
		chains.items[i].tasks.items[0].wcet = 7000;
		chains.items[i].tasks.items[0].reference_wcet = 7000;
		chains.items[i].tasks.items[0].utilization = 700000;
	}
	CHECK_EQ(mp_assign(&ctx, &chains, &cores), 0);
	CHECK(ctx.feasible);
	for (i = 0; i < 12; i++) {
		int controller = controller_of(&cores, i, 0);

		on_pcu += controller == MP_CONTROLLER_PCU;
		on_zcus += controller >= MP_CONTROLLER_ZCU1;
	}
	/* 70 % of the reference is 140 % of a ZCU core: new PCU cores */
	CHECK_EQ(on_pcu, 12);
	CHECK_EQ(on_zcus, 0);
	CHECK_EQ(cores.count, 32);
	mp_chain_array_free(&chains);
	mp_core_array_free(&cores);
}

/*
 * Affinity: the second fragment of a chain joins the core of the first
 * one if it fits, whatever the algorithm would choose otherwise.
 */
static int sibling_core(bool affinity)
{
	struct mp_chain_array chains = {0};
	struct mp_core_array cores;
	struct mp_chain chain;
	struct mp_ctx ctx;
	int i, core = -1;

	make_cores(&ctx, &cores, 3, MP_WFDU);
	ctx.params.affinity = affinity;
	chain = make_chain(5, 400000, 10000, MP_CONTROLLER_PCU);
	mp_core_add_chain(&cores.items[1], &chain, 10000);
	mp_chain_free(&chain);
	chain = make_chain(5, 100000, 10000, MP_CONTROLLER_PCU);
	chain.fragment_index = 1;
	mp_chain_array_push(&chains, &chain);
	mp_chain_free(&chain);
	CHECK_EQ(mp_assign(&ctx, &chains, &cores), 0);
	for (i = 0; i < cores.count; i++)
		if (cores.items[i].chains.count == 3)
			core = cores.items[i].id;
	mp_chain_array_free(&chains);
	mp_core_array_free(&cores);
	return core;
}

static void test_affinity(void)
{
	CHECK_EQ(sibling_core(true), 1);
	CHECK(sibling_core(false) != 1);	/* WFDU: an empty core */
}

/*
 * Spatial constraints of a placement: the controller must be allowed, its
 * memory must hold the fragment, and no replica may run on it (or on the
 * same core with MP_SEPARATE_CORES).
 */
static void test_placement_constraints(void)
{
	struct mp_core_array cores = {0};
	struct mp_chain a = make_chain(0, 100000, 10000, MP_CONTROLLER_ANY);
	struct mp_chain b = make_chain(1, 100000, 10000, MP_CONTROLLER_ANY);
	struct mp_ctx ctx;
	int gcd;

	mp_ctx_init(&ctx, 1);
	ctx.params.chain_count = 2;
	ctx.next_chain_id = 2;
	mp_add_core(&ctx, &cores, MP_CONTROLLER_PCU);	/* core 0 */
	mp_add_core(&ctx, &cores, MP_CONTROLLER_PCU);	/* core 1 */
	mp_add_core(&ctx, &cores, MP_CONTROLLER_ZCU2);	/* core 2 */

	/* allowed controllers */
	a.allowed = ZONAL_ZONES;
	CHECK(!mp_placement_allowed(&ctx, &cores, &a, 0, NULL));
	CHECK(mp_placement_allowed(&ctx, &cores, &a, 2, NULL));
	a.allowed = 0;

	/* memory: 300 KB on the PCU, then 250 more is too much for 512 */
	a.memory = 300;
	b.memory = 250;
	mp_core_load_with_chain(&cores.items[0], &a, &gcd);
	mp_core_add_chain(&cores.items[0], &a, gcd);
	ctx.params.pcu_memory = 512;
	CHECK(!mp_placement_allowed(&ctx, &cores, &b, 1, NULL));
	CHECK(mp_placement_allowed(&ctx, &cores, &b, 2, NULL));
	/* unless chain 0 leaves the PCU in an exchange */
	CHECK(mp_placement_allowed(&ctx, &cores, &b, 0, &a));
	CHECK_EQ(mp_controller_memory(&cores, MP_CONTROLLER_PCU), 300);
	ctx.params.pcu_memory = 0;

	/* replicas: chain 1 is a replica of chain 0, which is on core 0 */
	cores.items[0].chains.items[1].replica_group = 1;
	b.replica_group = 1;
	CHECK(!mp_placement_allowed(&ctx, &cores, &b, 1, NULL));
	CHECK(mp_placement_allowed(&ctx, &cores, &b, 2, NULL));
	ctx.params.separation = MP_SEPARATE_CORES;
	CHECK(mp_placement_allowed(&ctx, &cores, &b, 1, NULL));
	CHECK(!mp_placement_allowed(&ctx, &cores, &b, 0, NULL));

	mp_chain_free(&a);
	mp_chain_free(&b);
	mp_core_array_free(&cores);
}

/*
 * A free fragment that may only run on the zone controllers gets a new
 * core on the first allowed zone controller when the existing cores are
 * full, not on the PCU.
 */
static void test_new_core_for_zone_only_fragment(void)
{
	struct mp_chain_array chains = {0};
	struct mp_core_array cores = {0};
	struct mp_chain chain = make_chain(0, 300000, 10000, MP_CONTROLLER_ANY);
	struct mp_ctx ctx;

	chain.allowed = MP_CONTROLLER_BIT(MP_CONTROLLER_ZCU3);
	chain.tasks.items[0].allowed = chain.allowed;
	mp_ctx_init(&ctx, 1);
	ctx.params.chain_count = 1;
	ctx.next_chain_id = 1;
	mp_add_core(&ctx, &cores, MP_CONTROLLER_PCU);
	mp_chain_array_push(&chains, &chain);
	CHECK_EQ(mp_assign(&ctx, &chains, &cores), 0);
	CHECK(ctx.feasible);
	CHECK_EQ(cores.count, 2);
	CHECK_EQ(cores.items[1].controller, MP_CONTROLLER_ZCU3);
	CHECK_EQ(cores.items[1].chains.count, 2);
	mp_chain_free(&chain);
	mp_chain_array_free(&chains);
	mp_core_array_free(&cores);
}

/*
 * Core types: a task with a factor of 80 % on big cores and 150 % on
 * microcontroller and lock-step cores; reference WCET 1000 us.
 *   big (speed 100):    1000 x 80 / 100  = 800
 *   little (speed 40):  1000 x 100 / 40  = 2500 (no factor: 100 %)
 *   MCU (speed 50):     1000 x 150 / 50  = 3000
 * The best type on the PCU is big; a task allowed only on lock-step cores
 * cannot use the PCU at all.
 */
static void test_core_types(void)
{
	struct mp_core_array cores = {0};
	struct mp_chain chain = make_chain(0, 100000, 10000, MP_CONTROLLER_ANY);
	struct mp_task *task = &chain.tasks.items[0];
	struct mp_ctx ctx;

	task->type_factor[MP_CORE_BIG] = 80;
	task->type_factor[MP_CORE_MCU] = 150;
	task->type_factor[MP_CORE_LOCKSTEP] = 150;	/* same design */
	CHECK_EQ(mp_task_wcet_on(task, 100, MP_CORE_BIG), 800);
	CHECK_EQ(mp_task_wcet_on(task, 40, MP_CORE_LITTLE), 2500);
	CHECK_EQ(mp_task_wcet_on(task, 50, MP_CORE_MCU), 3000);

	mp_ctx_init(&ctx, 1);
	ctx.params.chain_count = 1;
	ctx.next_chain_id = 1;
	CHECK_EQ(mp_ecu_speed(&ctx.params, MP_CONTROLLER_PCU, MP_CORE_LITTLE), 40);
	CHECK_EQ(mp_best_core_type(&ctx.params, &chain, MP_CONTROLLER_PCU),
		 MP_CORE_BIG);
	CHECK_EQ(mp_best_core_type(&ctx.params, &chain, MP_CONTROLLER_ZCU1),
		 MP_CORE_MCU);
	mp_add_typed_core(&ctx, &cores, MP_CONTROLLER_PCU, MP_CORE_LITTLE);
	mp_add_typed_core(&ctx, &cores, MP_CONTROLLER_ZCU1, MP_CORE_LOCKSTEP);
	CHECK_EQ(cores.items[0].speed, 40);
	CHECK_EQ(cores.items[0].let_wcet, 125);	/* 50 us at 40 % */
	CHECK(mp_placement_allowed(&ctx, &cores, &chain, 0, NULL));

	task->types = 1u << MP_CORE_LOCKSTEP;
	CHECK_EQ(mp_best_core_type(&ctx.params, &chain, MP_CONTROLLER_PCU), -1);
	CHECK(!mp_placement_allowed(&ctx, &cores, &chain, 0, NULL));
	CHECK(mp_placement_allowed(&ctx, &cores, &chain, 1, NULL));
	mp_chain_free(&chain);
	mp_core_array_free(&cores);
}

/* a chain larger than phi with its LET task cannot be placed */
static void test_unfeasible(void)
{
	struct mp_chain_array chains = {0};
	struct mp_core_array cores;
	struct mp_chain chain = make_chain(0, 790000, 1000, MP_CONTROLLER_ANY);
	struct mp_ctx ctx;

	make_cores(&ctx, &cores, 1, MP_WFDU);
	mp_chain_array_push(&chains, &chain);
	CHECK_EQ(mp_assign(&ctx, &chains, &cores), 0);
	CHECK(!ctx.feasible);
	CHECK_EQ(cores.count, 2);	/* one core was tried in vain */
	mp_chain_free(&chain);
	mp_chain_array_free(&chains);
	mp_core_array_free(&cores);
}

/*
 * A free fragment between two fragments bound to ZCU2: the other
 * algorithms send it to the central computer (two network hops), CADU
 * keeps it in the zone (none).
 */
static int controller_between_bound(enum mp_algorithm algorithm)
{
	struct mp_chain_array chains = {0};
	struct mp_core_array cores = {0};
	struct mp_ctx ctx;
	int f, controller;

	mp_ctx_init(&ctx, 1);
	ctx.params.algorithm = algorithm;
	ctx.params.affinity = false;
	ctx.params.chain_count = 1;
	CHECK_EQ(mp_architecture_init(&ctx, &cores), 0);
	for (f = 0; f < 3; f++) {
		struct mp_chain chain = make_chain(0, 50000, 10000,
						   f == 1 ? MP_CONTROLLER_ANY :
						   MP_CONTROLLER_ZCU2);

		chain.fragment_index = f;
		chain.is_fragment = true;
		mp_chain_array_push(&chains, &chain);
		mp_chain_free(&chain);
	}
	CHECK_EQ(mp_assign(&ctx, &chains, &cores), 0);
	CHECK(ctx.feasible);
	controller = controller_of(&cores, 0, 1);
	mp_chain_array_free(&chains);
	mp_core_array_free(&cores);
	return controller;
}

static void test_communication_aware(void)
{
	CHECK_EQ(controller_between_bound(MP_FFDU), MP_CONTROLLER_PCU);
	CHECK_EQ(controller_between_bound(MP_BFDU), MP_CONTROLLER_PCU);
	CHECK_EQ(controller_between_bound(MP_CADU), MP_CONTROLLER_ZCU2);
}


/*
 * shared-resource contention: 500 memory requests of a task that wait at
 * most 100 ns each (two other cores at 50 ns) add 50 us to its WCET on
 * the core, whatever the speed of the core
 */
static void test_contention_wcet(void)
{
	struct mp_task task;
	struct mp_core core;
	struct mp_params params;

	memset(&task, 0, sizeof(task));
	memset(&core, 0, sizeof(core));
	mp_params_default(&params);
	params.memory_latency = 50;
	task.reference_wcet = task.wcet = 1000;
	task.period = 10000;
	task.accesses = mp_default_accesses(&params, task.reference_wcet);
	CHECK_EQ(task.accesses, 1000);
	task.accesses = 500;
	core.speed = 50;
	core.type = MP_CORE_MCU;
	core.access_stall = mp_access_stall(&params, 3);
	CHECK_EQ(core.access_stall, 100);
	CHECK_EQ(mp_task_wcet_on_core(&task, &core), 2000 + 50);
	core.access_stall = mp_access_stall(&params, 1);
	CHECK_EQ(mp_task_wcet_on_core(&task, &core), 2000);
	params.pcu_cores = 8;
	CHECK_EQ(mp_competing_cores(&params, MP_CONTROLLER_PCU), 8);
	params.competing_cores[MP_CONTROLLER_PCU] = 10;
	CHECK_EQ(mp_competing_cores(&params, MP_CONTROLLER_PCU), 10);
}

int main(void)
{
	RUN_TEST(test_contention_wcet);
	RUN_TEST(test_let_utilization);
	RUN_TEST(test_let_update_and_load);
	RUN_TEST(test_load_with_chain);
	RUN_TEST(test_core_speed);
	RUN_TEST(test_bin_packing_choices);
	RUN_TEST(test_controller_respected_and_core_added);
	RUN_TEST(test_free_fragments_prefer_pcu);
	RUN_TEST(test_affinity);
	RUN_TEST(test_communication_aware);
	RUN_TEST(test_core_types);
	RUN_TEST(test_placement_constraints);
	RUN_TEST(test_new_core_for_zone_only_fragment);
	RUN_TEST(test_unfeasible);
	return unit_summary();
}
