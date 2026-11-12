// SPDX-License-Identifier: Apache-2.0
/* Unit tests of the growable-array helpers (array.h). */
#include <errno.h>
#include <stdlib.h>

#include "mapping/array.h"
#include "unit.h"

/* a typed array as used by the model: items, count, capacity */
struct int_array {
	int *items;
	int count;
	int capacity;
};

static int push(struct int_array *array, int value)
{
	return mp_array_insert((void **)&array->items, &array->count,
			       &array->capacity, array->count, &value,
			       sizeof(value));
}

/* many pushes keep the order and grow the capacity */
static void test_push_keeps_order(void)
{
	struct int_array array = {0};
	int i, in_order = 1;

	for (i = 0; i < 1000; i++)
		CHECK_EQ(push(&array, i), 0);
	CHECK_EQ(array.count, 1000);
	CHECK(array.capacity >= 1000);
	for (i = 0; i < 1000; i++)
		if (array.items[i] != i)
			in_order = 0;
	CHECK(in_order);
	free(array.items);
}

/* insertion at the front, middle and end shifts the right elements */
static void test_insert_positions(void)
{
	struct int_array array = {0};
	int value;

	push(&array, 1);
	push(&array, 3);
	value = 0;
	CHECK_EQ(mp_array_insert((void **)&array.items, &array.count,
				 &array.capacity, 0, &value, sizeof(int)), 0);
	value = 2;
	CHECK_EQ(mp_array_insert((void **)&array.items, &array.count,
				 &array.capacity, 2, &value, sizeof(int)), 0);
	value = 4;
	CHECK_EQ(mp_array_insert((void **)&array.items, &array.count,
				 &array.capacity, 4, &value, sizeof(int)), 0);
	CHECK_EQ(array.count, 5);
	for (value = 0; value < 5; value++)
		CHECK_EQ(array.items[value], value);
	free(array.items);
}

/* removal shifts left; out-of-range indices are rejected */
static void test_remove(void)
{
	struct int_array array = {0};
	int i;

	for (i = 0; i < 5; i++)
		push(&array, i * 10);
	CHECK_EQ(mp_array_remove(array.items, &array.count, 1, sizeof(int)), 0);
	CHECK_EQ(array.count, 4);
	CHECK_EQ(array.items[0], 0);
	CHECK_EQ(array.items[1], 20);
	CHECK_EQ(array.items[3], 40);
	CHECK_EQ(mp_array_remove(array.items, &array.count, 4, sizeof(int)),
		 -EINVAL);
	CHECK_EQ(mp_array_remove(array.items, &array.count, -1, sizeof(int)),
		 -EINVAL);
	free(array.items);
}

/* reserving less than the capacity changes nothing */
static void test_reserve_is_idempotent(void)
{
	struct int_array array = {0};
	int *before;

	CHECK_EQ(mp_array_reserve((void **)&array.items, &array.capacity, 20,
				  sizeof(int)), 0);
	before = array.items;
	CHECK_EQ(mp_array_reserve((void **)&array.items, &array.capacity, 10,
				  sizeof(int)), 0);
	CHECK(array.items == before);
	CHECK(array.capacity >= 20);
	free(array.items);
}

int main(void)
{
	RUN_TEST(test_push_keeps_order);
	RUN_TEST(test_insert_positions);
	RUN_TEST(test_remove);
	RUN_TEST(test_reserve_is_idempotent);
	return unit_summary();
}
