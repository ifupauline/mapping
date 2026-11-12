// SPDX-License-Identifier: Apache-2.0
/* The metaheuristic baselines (search.h). */
#include <string.h>

#include "mapping/generator.h"
#include "mapping/search.h"
#include "unit.h"

/* a feasible, schedulable mapping, within the bounds, deterministic */
static void check_method(enum mp_search_method method, long budget)
{
	struct mp_chain_array application;
	struct mp_gen_params generator;
	struct mp_design_point a, b, heuristic;
	struct mp_search_params sp;
	struct mp_search_trace trace;
	struct mp_ctx ctx;
	int i, t;

	mp_ctx_init(&ctx, 4);
	mp_gen_params_default(&generator);
	generator.chain_count = 40;
	generator.replica_percent = 10;
	CHECK_EQ(mp_generate_application(&ctx, &generator, &application), 0);
	mp_search_params_default(&sp, method, budget);
	CHECK_EQ(mp_search(&ctx, &application, &sp, &a, &trace), 0);
	CHECK(a.feasible);
	CHECK_NEAR(a.stats.rate_final, 100, 1e-3);	/* every core */
	CHECK(a.stats.used_cores >= a.stats.lower_bound_cores);
	CHECK(trace.count >= 1);
	for (t = 1; t < trace.count; t++) {
		CHECK(trace.cores[t] < trace.cores[t - 1]);
		CHECK(trace.time[t] >= trace.time[t - 1]);
	}
	CHECK_EQ(trace.cores[trace.count - 1], a.stats.used_cores);
	/* every sub-chain once, on a core whose ECU allows it */
	{
		int placed = 0, j;

		for (i = 0; i < a.cores.count; i++)
			for (j = 0; j < a.cores.items[i].chains.count; j++) {
				const struct mp_chain *c =
					&a.cores.items[i].chains.items[j];

				if (c->is_let)
					continue;
				placed++;
				CHECK(mp_chain_allowed(c) &
				      MP_CONTROLLER_BIT(a.cores.items[i].controller));
				CHECK(a.cores.items[i].load <=
				      a.cores.items[i].fill_limit);
			}
		CHECK_EQ(placed, a.fragments.count);
	}
	/* the same sub-chains as the heuristic */
	CHECK_EQ(mp_run_design_point(&ctx, &application, NULL, &heuristic), 0);
	CHECK_EQ(a.fragments.count, heuristic.fragments.count);
	/* the same seed, the same result */
	CHECK_EQ(mp_search(&ctx, &application, &sp, &b, NULL), 0);
	CHECK_EQ(b.stats.used_cores, a.stats.used_cores);
	CHECK_EQ(b.communication.on_time_chains, a.communication.on_time_chains);
	mp_design_point_free(&a);
	mp_design_point_free(&b);
	mp_design_point_free(&heuristic);
	mp_chain_array_free(&application);
}

static void test_annealing(void)
{
	check_method(MP_SEARCH_ANNEALING, 5000);
}

static void test_genetic(void)
{
	check_method(MP_SEARCH_GENETIC, 3);
}

/* no contention model in the baselines */
static void test_contention_refused(void)
{
	struct mp_chain_array application;
	struct mp_gen_params generator;
	struct mp_design_point point;
	struct mp_search_params sp;
	struct mp_ctx ctx;

	mp_ctx_init(&ctx, 1);
	mp_gen_params_default(&generator);
	generator.chain_count = 5;
	CHECK_EQ(mp_generate_application(&ctx, &generator, &application), 0);
	ctx.params.memory_latency = 10;
	mp_search_params_default(&sp, MP_SEARCH_ANNEALING, 10);
	CHECK(mp_search(&ctx, &application, &sp, &point, NULL) < 0);
	mp_design_point_free(&point);
	mp_chain_array_free(&application);
}

int main(void)
{
	RUN_TEST(test_annealing);
	RUN_TEST(test_genetic);
	RUN_TEST(test_contention_refused);
	return unit_summary();
}
