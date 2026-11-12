// SPDX-License-Identifier: Apache-2.0
/*
 * Unit tests of the random generator. The expected sequences are the
 * reference output of xoshiro256** seeded with splitmix64: any platform
 * must draw the same numbers, otherwise no golden result could match.
 */
#include "mapping/rng.h"
#include "unit.h"

/* raw 64-bit output for seed 1 */
static void test_reference_sequence(void)
{
	static const uint64_t expected[] = {
		0xb3f2af6d0fc710c5ULL, 0x853b559647364ceaULL,
		0x92f89756082a4514ULL, 0x642e1c7bc266a3a7ULL,
	};
	struct mp_rng rng;
	int i;

	mp_rng_seed(&rng, 1);
	for (i = 0; i < 4; i++)
		CHECK(mp_rng_next(&rng) == expected[i]);
}

/* bounded draws for two seeds and two ranges */
static void test_reference_ranges(void)
{
	static const int expected_small[] = {7, 7, 6, 6, 5, 1, 11, 4, 11, 6, 2, 2};
	static const int expected_large[] = {289011, 353445, 779481, 835971,
					     247339, 604982};
	struct mp_rng rng;
	int i;

	mp_rng_seed(&rng, 42);
	for (i = 0; i < 12; i++)
		CHECK_EQ(mp_rng_range(&rng, 1, 12), expected_small[i]);

	mp_rng_seed(&rng, 7);
	for (i = 0; i < 6; i++)
		CHECK_EQ(mp_rng_range(&rng, -5, 1000000), expected_large[i]);
}

/* every value of a small range appears, none outside it, roughly uniform */
static void test_range_bounds_and_uniformity(void)
{
	int histogram[6] = {0};
	struct mp_rng rng;
	int i, outside = 0;

	mp_rng_seed(&rng, 2024);
	for (i = 0; i < 60000; i++) {
		int value = mp_rng_range(&rng, 0, 5);

		if (value < 0 || value > 5)
			outside++;
		else
			histogram[value]++;
	}
	CHECK_EQ(outside, 0);
	/* 10000 expected per value; 5 sigma is about 456 */
	for (i = 0; i < 6; i++)
		CHECK(histogram[i] > 9500 && histogram[i] < 10500);
}

/* a range of one value always returns it */
static void test_single_value_range(void)
{
	struct mp_rng rng;

	mp_rng_seed(&rng, 3);
	CHECK_EQ(mp_rng_range(&rng, 17, 17), 17);
	CHECK_EQ(mp_rng_range(&rng, -4, -4), -4);
}

int main(void)
{
	RUN_TEST(test_reference_sequence);
	RUN_TEST(test_reference_ranges);
	RUN_TEST(test_range_bounds_and_uniformity);
	RUN_TEST(test_single_value_range);
	return unit_summary();
}
