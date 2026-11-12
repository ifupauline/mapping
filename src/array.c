// SPDX-License-Identifier: Apache-2.0
/*
 * Growable arrays: storage management shared by the typed arrays of the
 * model (see array.h).
 */
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "mapping/array.h"

/* first allocation size, then doubled each time the array is full */
#define FIRST_CAPACITY 8

int mp_array_reserve(void **items, int *capacity, int needed,
		     size_t element_size)
{
	int new_capacity;
	void *new_items;

	if (needed <= *capacity)
		return 0;

	new_capacity = *capacity ? *capacity : FIRST_CAPACITY;
	while (new_capacity < needed)
		new_capacity *= 2;

	new_items = realloc(*items, (size_t)new_capacity * element_size);
	if (!new_items)
		return -ENOMEM;

	*items = new_items;
	*capacity = new_capacity;
	return 0;
}

int mp_array_insert(void **items, int *count, int *capacity, int index,
		    const void *element, size_t element_size)
{
	char *bytes;
	int err;

	if (index < 0 || index > *count)
		return -EINVAL;

	err = mp_array_reserve(items, capacity, *count + 1, element_size);
	if (err)
		return err;

	bytes = *items;
	/* shift the elements after @index one place to the right */
	memmove(bytes + (size_t)(index + 1) * element_size,
		bytes + (size_t)index * element_size,
		(size_t)(*count - index) * element_size);
	memcpy(bytes + (size_t)index * element_size, element, element_size);
	(*count)++;
	return 0;
}

int mp_array_remove(void *items, int *count, int index, size_t element_size)
{
	char *bytes = items;

	if (index < 0 || index >= *count)
		return -EINVAL;

	/* shift the elements after @index one place to the left */
	memmove(bytes + (size_t)index * element_size,
		bytes + (size_t)(index + 1) * element_size,
		(size_t)(*count - index - 1) * element_size);
	(*count)--;
	return 0;
}
