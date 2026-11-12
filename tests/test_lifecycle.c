// SPDX-License-Identifier: Apache-2.0
/* Unit tests of the lifecycle: software updates on a fixed platform. */
#include <string.h>

#include "mapping/assign.h"
#include "mapping/lifecycle.h"
#include "unit.h"

/*
 * A platform sized at phi = 0.5 for 100 task-chains absorbs updates of 10
 * task-chains on the cores of its SOP mapping only; one sized at 0.8
 * absorbs fewer. The SOP is the design point of the methodology.
 */
static void test_updates_on_fixed_hardware(void)
{
	struct mp_lifecycle_result roomy, tight;
	struct mp_lifecycle_params params;
	struct mp_gen_params generator;
	struct mp_chain_array application;
	struct mp_design_point sop;
	struct mp_ctx ctx;
	int u;

	mp_lifecycle_params_default(&params);
	params.updates = 10;
	params.chains_per_update = 10;
	mp_gen_params_default(&generator);
	generator.chain_count = 100;
	mp_ctx_init(&ctx, 7);
	CHECK_EQ(mp_generate_application(&ctx, &generator, &application), 0);
	ctx.params.fill_limit = 500000;
	CHECK_EQ(mp_run_lifecycle(&ctx, &application, &generator, &params, 11,
				  &sop, &roomy), 0);
	CHECK(sop.feasible);
	CHECK_EQ(roomy.platform_cores, sop.stats.used_cores);
	CHECK(roomy.absorbed >= 1);
	for (u = 0; u <= roomy.absorbed; u++) {
		CHECK_EQ(roomy.chains[u], 100 + 10 * u);
		CHECK(roomy.scaling[u] >= 1.0f);
		if (u)
			CHECK(roomy.load[u] > roomy.load[u - 1]);
	}
	mp_design_point_free(&sop);

	ctx.params.fill_limit = 800000;
	CHECK_EQ(mp_run_lifecycle(&ctx, &application, &generator, &params, 11,
				  &sop, &tight), 0);
	CHECK(tight.platform_cores <= roomy.platform_cores);
	CHECK(tight.absorbed <= roomy.absorbed);
	mp_design_point_free(&sop);
	mp_chain_array_free(&application);
}

int main(void)
{
	RUN_TEST(test_updates_on_fixed_hardware);
	return unit_summary();
}
