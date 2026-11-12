/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MAPPING_UNIT_H
#define MAPPING_UNIT_H

/* the zone controllers of the built-in zonal platform, as a mask */
#define ZONAL_ZONES	0x3eu

/*
 * Minimal unit-test helper. A test file defines functions that use the
 * CHECK macros, and a main() that runs them with RUN_TEST() and returns
 * unit_summary(). A failed check prints its location and values and the
 * test continues, so one run reports every failure.
 */
#include <math.h>
#include <stdio.h>

static int unit_checks;		/* checks evaluated */
static int unit_failures;	/* checks failed */

#define CHECK(condition)						\
	do {								\
		unit_checks++;						\
		if (!(condition)) {					\
			unit_failures++;				\
			printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__,	\
			       #condition);				\
		}							\
	} while (0)

/* integers (any integer type up to long long) */
#define CHECK_EQ(actual, expected)					\
	do {								\
		long long actual_value = (long long)(actual);		\
		long long expected_value = (long long)(expected);	\
		unit_checks++;						\
		if (actual_value != expected_value) {			\
			unit_failures++;				\
			printf("  FAIL %s:%d: %s = %lld, expected %lld\n",	\
			       __FILE__, __LINE__, #actual, actual_value,	\
			       expected_value);				\
		}							\
	} while (0)

/* floating point, within an absolute tolerance */
#define CHECK_NEAR(actual, expected, tolerance)				\
	do {								\
		double actual_value = (double)(actual);			\
		double expected_value = (double)(expected);		\
		unit_checks++;						\
		if (fabs(actual_value - expected_value) > (tolerance)) {	\
			unit_failures++;				\
			printf("  FAIL %s:%d: %s = %.9g, expected %.9g\n",	\
			       __FILE__, __LINE__, #actual, actual_value,	\
			       expected_value);				\
		}							\
	} while (0)

#define RUN_TEST(test_function)						\
	do {								\
		int failures_before = unit_failures;			\
		test_function();					\
		printf("%-48s %s\n", #test_function,			\
		       unit_failures == failures_before ? "ok" : "FAILED");	\
	} while (0)

static inline int unit_summary(void)
{
	printf("%d checks, %d failed\n", unit_checks, unit_failures);
	return unit_failures ? 1 : 0;
}

#endif /* MAPPING_UNIT_H */
