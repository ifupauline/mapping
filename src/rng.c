// SPDX-License-Identifier: Apache-2.0
/*
 * Deterministic pseudo-random number generator: xoshiro256** seeded with
 * splitmix64 (see rng.h): the same numbers for the same seed on every
 * platform.
 */
#include "mapping/rng.h"

/* splitmix64: expands one 64-bit seed into well-mixed 64-bit words */
static uint64_t splitmix64(uint64_t *seed)
{
	uint64_t z = (*seed += 0x9e3779b97f4a7c15ULL);

	z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
	z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
	return z ^ (z >> 31);
}

static uint64_t rotate_left(uint64_t value, int shift)
{
	return (value << shift) | (value >> (64 - shift));
}

void mp_rng_seed(struct mp_rng *rng, uint64_t seed)
{
	int i;

	/* splitmix64 never yields four zero words, as xoshiro requires */
	for (i = 0; i < 4; i++)
		rng->state[i] = splitmix64(&seed);
}

uint64_t mp_rng_next(struct mp_rng *rng)
{
	uint64_t *s = rng->state;
	uint64_t result = rotate_left(s[1] * 5, 7) * 9;
	uint64_t t = s[1] << 17;

	s[2] ^= s[0];
	s[3] ^= s[1];
	s[1] ^= s[2];
	s[0] ^= s[3];
	s[2] ^= t;
	s[3] = rotate_left(s[3], 45);

	return result;
}

int mp_rng_range(struct mp_rng *rng, int min, int max)
{
	uint64_t span = (uint64_t)((int64_t)max - (int64_t)min) + 1;
	/* largest multiple of span that fits in 2^64: values above it are
	 * redrawn, so every result in [min, max] is equally likely */
	uint64_t limit = UINT64_MAX - (UINT64_MAX % span + 1) % span;
	uint64_t draw;

	do {
		draw = mp_rng_next(rng);
	} while (draw > limit);

	return (int)((int64_t)min + (int64_t)(draw % span));
}

double mp_rng_uniform(struct mp_rng *rng)
{
	/* the 53 high bits, shifted by half a step away from 0 */
	return ((double)(mp_rng_next(rng) >> 11) + 0.5) * 0x1.0p-53;
}
