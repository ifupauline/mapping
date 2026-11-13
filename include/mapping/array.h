/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MAPPING_ARRAY_H
#define MAPPING_ARRAY_H

#include <stddef.h>

/*
 * Growable arrays.
 *
 * Every array of the model is a plain structure with three fields:
 *
 *	struct mp_task_array {
 *		struct mp_task *items;	   elements, owned by the array
 *		int count;		   number of elements in use
 *		int capacity;		   number of elements allocated
 *	};
 *
 * The helpers below work on those three fields for any element type, so
 * the typed arrays (tasks, task-chains, cores) only add the deep copy and
 * free of their elements. An array initialized to all zeros is a valid
 * empty array.
 */

/**
 * mp_array_reserve() - make room for at least @needed elements
 * @items: address of the element pointer (reallocated if needed)
 * @capacity: address of the allocated capacity (updated)
 * @needed: number of elements the array must be able to hold
 * @element_size: size of one element in bytes
 *
 * Capacity grows geometrically, so repeated pushes cost amortized O(1).
 * Return: 0, or -ENOMEM (the array is then unchanged).
 */
int mp_array_reserve(void **items, int *capacity, int needed,
		     size_t element_size);

/**
 * mp_array_insert() - insert one element at @index, shifting the rest right
 * @element: the element to copy in (shallow copy of its bytes)
 *
 * @index may be @count (append). Return: 0, -EINVAL or -ENOMEM.
 */
int mp_array_insert(void **items, int *count, int *capacity, int index,
		    const void *element, size_t element_size);

/**
 * mp_array_remove() - remove the element at @index, shifting the rest left
 *
 * The removed element is not freed: the caller frees what it owns first.
 * Return: 0 or -EINVAL.
 */
int mp_array_remove(void *items, int *count, int index, size_t element_size);

#endif /* MAPPING_ARRAY_H */
