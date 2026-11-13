/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MAPPING_RNG_H
#define MAPPING_RNG_H

#include <stdint.h>

/*
 * Deterministic pseudo-random number generator.
 *
 * xoshiro256** (Blackman and Vigna, 2018) seeded with splitmix64, and an
 * unbiased bounded integer draw by rejection. Written out in full so that
 * every platform draws exactly the same numbers for the same seed (the
 * results, and the golden files, are reproducible). Each design point owns
 * its own generator (in struct mp_ctx), so design points can run in
 * parallel.
 */
struct mp_rng {
	uint64_t state[4];	/* xoshiro256** state, never all zero */
};

/* Initialize @rng from a 64-bit seed (any value, including 0). */
void mp_rng_seed(struct mp_rng *rng, uint64_t seed);

/* Next 64 random bits. */
uint64_t mp_rng_next(struct mp_rng *rng);

/* Uniform integer in [min, max] (both included), without modulo bias. */
int mp_rng_range(struct mp_rng *rng, int min, int max);

/* Uniform real in (0, 1): 53 random bits, never exactly 0 or 1. */
double mp_rng_uniform(struct mp_rng *rng);

#endif /* MAPPING_RNG_H */
