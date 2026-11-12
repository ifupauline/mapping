// SPDX-License-Identifier: Apache-2.0
/* Unit tests of the generator (periods, UUniFast, chain structure). */
#include <errno.h>
#include <math.h>
#include <string.h>

#include "mapping/generator.h"
#include "unit.h"

/* over many draws, every period appears with its WATERS 2015 share */
static void test_period_shares(void)
{
	static const int periods[] = {1000, 2000, 5000, 10000, 20000, 50000,
				      100000, 200000, 1000000};
	static const int shares[] = {3, 2, 2, 25, 25, 3, 20, 1, 4};
	int counts[9] = {0}, draws = 170000, i, k;
	struct mp_rng rng;

	mp_rng_seed(&rng, 7);
	for (i = 0; i < draws; i++) {
		int period = mp_draw_waters_period(&rng);

		for (k = 0; k < 9; k++)
			if (period == periods[k])
				counts[k]++;
	}
	for (k = 0; k < 9; k++)
		/* share among the periodic runnables (85 %), within 0.5 % */
		CHECK_NEAR((double)counts[k] / draws, shares[k] / 85.0, 0.005);
}

/* UUniFast: exact sum, positive values, mean total / count */
static void test_uunifast(void)
{
	double shares[5], mean_first = 0;
	struct mp_rng rng;
	int i, k;

	mp_rng_seed(&rng, 3);
	for (i = 0; i < 20000; i++) {
		double sum = 0;

		mp_uunifast(&rng, 600, 5, shares);
		for (k = 0; k < 5; k++) {
			CHECK(shares[k] > 0);
			sum += shares[k];
		}
		CHECK_NEAR(sum, 600, 1e-9);
		mean_first += shares[0];
	}
	/* by symmetry each value has mean 600 / 5 */
	CHECK_NEAR(mean_first / 20000, 120, 2);
}

/* number of activation patterns of @chain (runs of equal periods) */
static int pattern_count(const struct mp_chain *chain)
{
	int k, count = 1;

	for (k = 1; k < chain->tasks.count; k++)
		count += chain->tasks.items[k].period !=
			 chain->tasks.items[k - 1].period;
	return count;
}

/*
 * The structure of the generated chains: WATERS 2015 pattern and task
 * counts, sorted and numbered chains, consistent utilizations, bindings
 * (local chains in one zone, otherwise only the first and last tasks
 * bound, and never both of a 2-task chain), deadlines and messages.
 */
static void test_application_structure(void)
{
	struct mp_chain_array application;
	struct mp_gen_params params;
	int patterns[4] = {0}, local = 0, sensors = 0, actuators = 0;
	int longer = 0;
	int i, j, large = 0, tasks = 0;
	struct mp_ctx ctx;

	mp_gen_params_default(&params);
	params.chain_count = 2000;
	mp_ctx_init(&ctx, 11);
	CHECK_EQ(mp_generate_application(&ctx, &params, &application), 0);
	CHECK_EQ(application.count, 2000);
	CHECK_EQ(ctx.params.chain_count, 2000);
	for (i = 0; i < application.count; i++) {
		const struct mp_chain *chain = &application.items[i];
		int count = chain->tasks.count, p = pattern_count(chain);
		int64_t period_sum = 0;
		int sum = 0;

		CHECK_EQ(chain->id, i);
		if (i)
			CHECK(chain->utilization <=
			      application.items[i - 1].utilization);
		CHECK(p >= 1 && p <= 3);
		patterns[p]++;
		CHECK(count >= 2 * p && count <= 5 * p);
		for (j = 0; j < count; j++) {
			const struct mp_task *task = &chain->tasks.items[j];
			bool edge = j == 0 || j == count - 1;

			CHECK(task->wcet >= 1 && task->wcet < task->period);
			CHECK_EQ(task->reference_wcet, task->wcet);
			CHECK_EQ(task->utilization,
				 mp_utilization(task->wcet, task->period));
			CHECK_EQ(task->id_in_chain, j);
			CHECK_EQ(task->chain_id, i);
			CHECK(task->message_bytes >= 4 &&
			      task->message_bytes <= params.large_message_bytes);
			if (chain->controller != MP_CONTROLLER_ANY)
				CHECK_EQ(task->controller, chain->controller);
			else if (!edge)
				CHECK_EQ(task->controller, MP_CONTROLLER_ANY);
			if (j == 0 || task->period !=
			    chain->tasks.items[j - 1].period)
				period_sum += task->period;
			large += task->message_bytes > MP_GEN_FRAME_BYTES;
			sum += task->utilization;
			tasks++;
		}
		CHECK_EQ(chain->utilization, sum);
		CHECK_EQ(chain->deadline, (int)(period_sum *
						params.deadline_percent / 100));
		if (chain->controller != MP_CONTROLLER_ANY) {
			local++;
			continue;
		}
		sensors += chain->tasks.items[0].controller !=
			   MP_CONTROLLER_ANY;
		/* a chain of 2 tasks keeps one computation task */
		if (count == 2)
			CHECK(chain->tasks.items[0].controller ==
			      MP_CONTROLLER_ANY ||
			      chain->tasks.items[1].controller ==
			      MP_CONTROLLER_ANY);
		if (count == 2)
			continue;
		longer++;
		actuators += chain->tasks.items[count - 1].controller !=
			     MP_CONTROLLER_ANY;
	}
	/* the shares of the parameters, within a few percent */
	CHECK_NEAR(patterns[1] / 2000.0, 0.70, 0.03);
	CHECK_NEAR(patterns[2] / 2000.0, 0.20, 0.03);
	CHECK_NEAR(patterns[3] / 2000.0, 0.10, 0.03);
	CHECK_NEAR(local / 2000.0, params.local_percent / 100.0, 0.03);
	CHECK_NEAR((double)sensors / (2000 - local),
		   params.sensor_percent / 100.0, 0.04);
	CHECK_NEAR((double)actuators / longer,
		   params.actuator_percent / 100.0, 0.04);
	CHECK_NEAR((double)large / tasks, 0.05, 0.02);
	mp_chain_array_free(&application);

	/* inconsistent parameters are refused */
	params.chain_count = 0;
	CHECK_EQ(mp_generate_application(&ctx, &params, &application),
		 -EINVAL);
}

/*
 * Chain utilizations: log-uniform between the bounds (median the
 * geometric mean), a few heavy chains above, and no task above its cap.
 */
static void test_utilizations(void)
{
	struct mp_chain_array application;
	struct mp_gen_params params;
	int i, j, heavy = 0, below_median = 0, light = 0;
	double median;
	struct mp_ctx ctx;

	mp_gen_params_default(&params);
	params.chain_count = 3000;
	median = sqrt((double)params.min_chain_utilization *
		      params.max_chain_utilization);
	mp_ctx_init(&ctx, 4);
	CHECK_EQ(mp_generate_application(&ctx, &params, &application), 0);
	for (i = 0; i < application.count; i++) {
		const struct mp_chain *chain = &application.items[i];

		/* rounding C up to 1 us adds a little */
		CHECK(chain->utilization >= params.min_chain_utilization / 2);
		CHECK(chain->utilization <= params.max_heavy_utilization +
					    chain->tasks.count * 1000);
		if (chain->utilization > params.max_chain_utilization +
					 chain->tasks.count * 1000) {
			heavy++;
			continue;
		}
		light++;
		below_median += chain->utilization < median;
		for (j = 0; j < chain->tasks.count; j++)
			CHECK(chain->tasks.items[j].utilization <=
			      params.max_task_utilization + 1000);
	}
	CHECK(heavy > 0);
	CHECK_NEAR((double)heavy / application.count,
		   params.heavy_percent / 100.0, 0.01);
	CHECK_NEAR((double)below_median / light, 0.5, 0.04);
	mp_chain_array_free(&application);
}

/* same seed, same application; another seed, another one */
static void test_reproducible(void)
{
	struct mp_chain_array first, second, third;
	struct mp_gen_params params;
	struct mp_ctx ctx;
	int i, same = 1, differ = 0;

	mp_gen_params_default(&params);
	params.chain_count = 50;
	mp_ctx_init(&ctx, 5);
	mp_generate_application(&ctx, &params, &first);
	mp_ctx_init(&ctx, 5);
	mp_generate_application(&ctx, &params, &second);
	mp_ctx_init(&ctx, 6);
	mp_generate_application(&ctx, &params, &third);
	CHECK_EQ(first.count, second.count);
	for (i = 0; i < first.count && i < second.count; i++) {
		if (first.items[i].utilization != second.items[i].utilization ||
		    first.items[i].tasks.count != second.items[i].tasks.count)
			same = 0;
		differ += first.items[i].utilization !=
			  third.items[i].utilization;
	}
	CHECK(same);
	CHECK(differ > 0);
	mp_chain_array_free(&first);
	mp_chain_array_free(&second);
	mp_chain_array_free(&third);
}

/*
 * Constraints of the generator: task memories within their range,
 * co-located pairs of free tasks of one pattern, zone-only chains, and
 * replicas: the share asked, two chains per group with the same tasks,
 * bound in disjoint zones, their free tasks out of the other's zones.
 */
static void test_constraints(void)
{
	struct mp_chain_array application;
	struct mp_gen_params params;
	int groups[400] = {0}, i, j, k, colocated = 0, pairs = 0, zone_only = 0;
	struct mp_ctx ctx;

	mp_gen_params_default(&params);
	params.chain_count = 400;
	params.replica_percent = 20;
	params.colocate_percent = 30;
	params.zone_only_percent = 25;
	mp_ctx_init(&ctx, 9);
	CHECK_EQ(mp_generate_application(&ctx, &params, &application), 0);
	CHECK_EQ(application.count, 400);
	for (i = 0; i < application.count; i++) {
		const struct mp_chain *chain = &application.items[i];
		int memory = 0;

		for (k = 0; k < chain->tasks.count; k++) {
			const struct mp_task *task = &chain->tasks.items[k];

			CHECK(task->memory >= params.min_task_memory &&
			      task->memory <= params.max_task_memory);
			memory += task->memory;
			if (k + 1 < chain->tasks.count &&
			    task->controller == MP_CONTROLLER_ANY &&
			    chain->tasks.items[k + 1].controller ==
			    MP_CONTROLLER_ANY &&
			    task->period == chain->tasks.items[k + 1].period) {
				pairs++;
				colocated += task->colocate_next;
			} else {
				CHECK(!task->colocate_next);
			}
		}
		CHECK_EQ(chain->memory, memory);
		zone_only += chain->controller == MP_CONTROLLER_ANY &&
			     !(mp_chain_allowed(chain) &
			       MP_CONTROLLER_BIT(MP_CONTROLLER_PCU)) &&
			     !chain->replica_group;
		if (chain->replica_group > 0 && chain->replica_group < 400)
			groups[chain->replica_group]++;
	}
	CHECK_NEAR((double)colocated / pairs, 0.30, 0.05);
	CHECK(zone_only > 0);
	/* 20 % of 400 chains are replicas: 80 groups of two */
	for (j = 1; j <= 80; j++)
		CHECK_EQ(groups[j], 2);
	CHECK_EQ(groups[81], 0);

	/* the two chains of a group */
	for (i = 0; i < application.count; i++)
		for (j = i + 1; j < application.count; j++) {
			const struct mp_chain *a = &application.items[i];
			const struct mp_chain *b = &application.items[j];
			unsigned zones_a = 0, zones_b = 0;

			if (!a->replica_group || a->replica_group != b->replica_group)
				continue;
			CHECK_EQ(a->tasks.count, b->tasks.count);
			CHECK_EQ(a->utilization, b->utilization);
			for (k = 0; k < a->tasks.count; k++) {
				if (a->tasks.items[k].controller != MP_CONTROLLER_ANY)
					zones_a |= 1u << a->tasks.items[k].controller;
				if (b->tasks.items[k].controller != MP_CONTROLLER_ANY)
					zones_b |= 1u << b->tasks.items[k].controller;
			}
			CHECK_EQ(zones_a & zones_b, 0);
			for (k = 0; k < a->tasks.count; k++) {
				if (a->tasks.items[k].controller == MP_CONTROLLER_ANY)
					CHECK_EQ(mp_task_allowed(&a->tasks.items[k]) &
						 zones_b, 0);
				if (b->tasks.items[k].controller == MP_CONTROLLER_ANY)
					CHECK_EQ(mp_task_allowed(&b->tasks.items[k]) &
						 zones_a, 0);
			}
		}
	mp_chain_array_free(&application);
}

/* the detection chain: one chain of 9 tasks on the PCU */
static void test_detection_chain(void)
{
	struct mp_chain_array application;
	struct mp_ctx ctx;
	int j;

	mp_ctx_init(&ctx, 1);
	CHECK_EQ(mp_detection_chain_application(&ctx, &application), 0);
	CHECK_EQ(application.count, 1);
	CHECK_EQ(application.items[0].tasks.count, 9);
	for (j = 0; j < 9; j++) {
		const struct mp_task *task = &application.items[0].tasks.items[j];

		CHECK_EQ(task->controller, MP_CONTROLLER_PCU);
		CHECK(task->wcet < task->period);
		CHECK(task->message_bytes > 0);
	}
	mp_chain_array_free(&application);
}

int main(void)
{
	RUN_TEST(test_period_shares);
	RUN_TEST(test_uunifast);
	RUN_TEST(test_application_structure);
	RUN_TEST(test_utilizations);
	RUN_TEST(test_reproducible);
	RUN_TEST(test_constraints);
	RUN_TEST(test_detection_chain);
	return unit_summary();
}
