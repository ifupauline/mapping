// SPDX-License-Identifier: Apache-2.0
/*
 * System model: arrays of tasks, chains and cores with deep copy and free,
 * utilization and period GCD of a chain, stable sorting (see model.h).
 */
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "mapping/array.h"
#include "mapping/model.h"

/* ---- tasks ---------------------------------------------------------- */

int mp_task_array_push(struct mp_task_array *array, const struct mp_task *task)
{
	return mp_array_insert((void **)&array->items, &array->count,
			       &array->capacity, array->count, task,
			       sizeof(*task));
}

int mp_task_array_copy(struct mp_task_array *copy,
		       const struct mp_task_array *original)
{
	memset(copy, 0, sizeof(*copy));
	if (!original->count)
		return 0;
	if (mp_array_reserve((void **)&copy->items, &copy->capacity,
			     original->count, sizeof(*copy->items)))
		return -ENOMEM;
	memcpy(copy->items, original->items,
	       (size_t)original->count * sizeof(*copy->items));
	copy->count = original->count;
	return 0;
}

void mp_task_array_free(struct mp_task_array *array)
{
	free(array->items);
	memset(array, 0, sizeof(*array));
}

/* ---- chains --------------------------------------------------------- */

int mp_chain_copy(struct mp_chain *copy, const struct mp_chain *original)
{
	*copy = *original;
	return mp_task_array_copy(&copy->tasks, &original->tasks);
}

void mp_chain_free(struct mp_chain *chain)
{
	mp_task_array_free(&chain->tasks);
}

int mp_chain_array_push(struct mp_chain_array *array,
			const struct mp_chain *chain)
{
	struct mp_chain copy;
	int err;

	err = mp_chain_copy(&copy, chain);
	if (err)
		return err;
	err = mp_array_insert((void **)&array->items, &array->count,
			      &array->capacity, array->count, &copy,
			      sizeof(copy));
	if (err)
		mp_chain_free(&copy);
	return err;
}

int mp_chain_array_copy(struct mp_chain_array *copy,
			const struct mp_chain_array *original)
{
	int i, err;

	memset(copy, 0, sizeof(*copy));
	for (i = 0; i < original->count; i++) {
		err = mp_chain_array_push(copy, &original->items[i]);
		if (err) {
			mp_chain_array_free(copy);
			return err;
		}
	}
	return 0;
}

void mp_chain_array_free(struct mp_chain_array *array)
{
	int i;

	for (i = 0; i < array->count; i++)
		mp_chain_free(&array->items[i]);
	free(array->items);
	memset(array, 0, sizeof(*array));
}

void mp_chain_update_utilization(struct mp_chain *chain)
{
	int i;

	chain->utilization = 0;
	for (i = 0; i < chain->tasks.count; i++)
		chain->utilization += chain->tasks.items[i].utilization;
}

/* ---- cores ---------------------------------------------------------- */

int mp_core_copy(struct mp_core *copy, const struct mp_core *original)
{
	int err;

	*copy = *original;
	memset(&copy->chains, 0, sizeof(copy->chains));
	memset(&copy->tasks, 0, sizeof(copy->tasks));

	err = mp_chain_array_copy(&copy->chains, &original->chains);
	if (err)
		return err;
	err = mp_task_array_copy(&copy->tasks, &original->tasks);
	if (err)
		mp_chain_array_free(&copy->chains);
	return err;
}

void mp_core_free(struct mp_core *core)
{
	mp_chain_array_free(&core->chains);
	mp_task_array_free(&core->tasks);
}

int mp_core_array_push(struct mp_core_array *array, const struct mp_core *core)
{
	struct mp_core copy;
	int err;

	err = mp_core_copy(&copy, core);
	if (err)
		return err;
	err = mp_array_insert((void **)&array->items, &array->count,
			      &array->capacity, array->count, &copy,
			      sizeof(copy));
	if (err)
		mp_core_free(&copy);
	return err;
}

int mp_core_array_copy(struct mp_core_array *copy,
		       const struct mp_core_array *original)
{
	int i, err;

	memset(copy, 0, sizeof(*copy));
	for (i = 0; i < original->count; i++) {
		err = mp_core_array_push(copy, &original->items[i]);
		if (err) {
			mp_core_array_free(copy);
			return err;
		}
	}
	return 0;
}

void mp_core_array_free(struct mp_core_array *array)
{
	int i;

	for (i = 0; i < array->count; i++)
		mp_core_free(&array->items[i]);
	free(array->items);
	memset(array, 0, sizeof(*array));
}

/* ---- period GCD ----------------------------------------------------- */

/* Euclid's algorithm, as std::__gcd for non-negative values */
static int gcd_of(int a, int b)
{
	while (b) {
		int remainder = a % b;

		a = b;
		b = remainder;
	}
	return a;
}

int mp_tasks_period_gcd(const struct mp_task_array *tasks)
{
	int i, result;

	if (!tasks->count)
		return 0;
	result = tasks->items[0].period;
	for (i = 1; i < tasks->count; i++)
		result = gcd_of(result, tasks->items[i].period);
	return result;
}

/* ---- stable sort ---------------------------------------------------- */

/* merge the sorted runs [left, middle) and [middle, right) through @buffer */
static void merge_runs(char *items, char *buffer, int left, int middle,
		       int right, size_t size,
		       bool (*less)(const void *a, const void *b))
{
	int i = left, j = middle, out = left;

	while (i < middle && j < right) {
		/* take from the right run only if strictly smaller: stable */
		if (less(items + (size_t)j * size, items + (size_t)i * size))
			memcpy(buffer + (size_t)out++ * size,
			       items + (size_t)j++ * size, size);
		else
			memcpy(buffer + (size_t)out++ * size,
			       items + (size_t)i++ * size, size);
	}
	while (i < middle)
		memcpy(buffer + (size_t)out++ * size, items + (size_t)i++ * size,
		       size);
	while (j < right)
		memcpy(buffer + (size_t)out++ * size, items + (size_t)j++ * size,
		       size);
	memcpy(items + (size_t)left * size, buffer + (size_t)left * size,
	       (size_t)(right - left) * size);
}

static void merge_sort(char *items, char *buffer, int left, int right,
		       size_t size, bool (*less)(const void *a, const void *b))
{
	int middle;

	if (right - left < 2)
		return;
	if (right - left <= 8) {
		/* insertion sort of a short run (stable) */
		int i, j;

		for (i = left + 1; i < right; i++) {
			memcpy(buffer, items + (size_t)i * size, size);
			for (j = i; j > left &&
			     less(buffer, items + (size_t)(j - 1) * size); j--)
				memcpy(items + (size_t)j * size,
				       items + (size_t)(j - 1) * size, size);
			memcpy(items + (size_t)j * size, buffer, size);
		}
		return;
	}
	middle = left + (right - left) / 2;
	merge_sort(items, buffer, left, middle, size, less);
	merge_sort(items, buffer, middle, right, size, less);
	/* runs already in order (the common case): nothing to merge */
	if (!less(items + (size_t)middle * size,
		  items + (size_t)(middle - 1) * size))
		return;
	merge_runs(items, buffer, left, middle, right, size, less);
}

int mp_stable_sort(void *items, int count, size_t element_size,
		   bool (*less)(const void *a, const void *b))
{
	char *buffer;

	if (count < 2)
		return 0;
	buffer = malloc((size_t)count * element_size);
	if (!buffer)
		return -ENOMEM;
	merge_sort(items, buffer, 0, count, element_size, less);
	free(buffer);
	return 0;
}

/*
 * Comparators: ties are broken by identity fields, LET first, then chain
 * id, then task id or fragment index.
 */
static bool task_identity_less(const struct mp_task *a, const struct mp_task *b)
{
	if (a->is_let != b->is_let)
		return a->is_let;
	if (a->chain_id != b->chain_id)
		return a->chain_id < b->chain_id;
	return a->id_in_chain < b->id_in_chain;
}

static bool chain_identity_less(const struct mp_chain *a,
				const struct mp_chain *b)
{
	if (a->is_let != b->is_let)
		return a->is_let;
	if (a->id != b->id)
		return a->id < b->id;
	return a->fragment_index < b->fragment_index;
}

static bool task_priority_less(const void *a, const void *b)
{
	const struct mp_task *first = a, *second = b;

	if (first->priority != second->priority)
		return first->priority < second->priority;
	return task_identity_less(first, second);
}

static bool task_id_in_chain_less(const void *a, const void *b)
{
	const struct mp_task *first = a, *second = b;

	if (first->id_in_chain != second->id_in_chain)
		return first->id_in_chain < second->id_in_chain;
	return task_identity_less(first, second);
}

static bool chain_decreasing_utilization_less(const void *a, const void *b)
{
	const struct mp_chain *first = a, *second = b;

	if (first->utilization != second->utilization)
		return first->utilization > second->utilization;
	return chain_identity_less(first, second);
}

static bool task_chain_less(const void *a, const void *b)
{
	return task_identity_less(a, b);
}

int mp_sort_tasks_by_chain(struct mp_task_array *tasks)
{
	return mp_stable_sort(tasks->items, tasks->count, sizeof(*tasks->items),
			      task_chain_less);
}

int mp_sort_tasks_by_priority(struct mp_task_array *tasks)
{
	return mp_stable_sort(tasks->items, tasks->count, sizeof(*tasks->items),
			      task_priority_less);
}

int mp_sort_tasks_by_id_in_chain(struct mp_task_array *tasks)
{
	return mp_stable_sort(tasks->items, tasks->count, sizeof(*tasks->items),
			      task_id_in_chain_less);
}

int mp_sort_chains_by_decreasing_utilization(struct mp_chain_array *chains)
{
	return mp_stable_sort(chains->items, chains->count,
			      sizeof(*chains->items),
			      chain_decreasing_utilization_less);
}

static bool core_increasing_room_less(const void *a, const void *b)
{
	const struct mp_core *first = a, *second = b;

	if (first->load_remaining != second->load_remaining)
		return first->load_remaining < second->load_remaining;
	return first->id < second->id;
}

int mp_sort_cores_by_increasing_room(struct mp_core_array *cores)
{
	return mp_stable_sort(cores->items, cores->count, sizeof(*cores->items),
			      core_increasing_room_less);
}

int mp_utilization(int wcet, int period)
{
	if (period <= 0)
		return 0;
	return (int)(((int64_t)wcet * MP_UTILIZATION_UNIT + period - 1) / period);
}

int mp_task_type_factor(const struct mp_task *task, int type)
{
	if (type < 0 || type >= MP_CORE_TYPES || !task->type_factor[type])
		return 100;
	return task->type_factor[type];
}

int mp_task_wcet_on(const struct mp_task *task, int speed, int type)
{
	int64_t reference = task->reference_wcet ? task->reference_wcet :
			    task->wcet;
	int64_t factor = mp_task_type_factor(task, type);

	/*
	 * a slower core takes longer, and the task may gain or lose on
	 * this type of core: C = C_ref x factor / speed; at least 1 us
	 */
	reference = (reference * factor + speed - 1) / speed;
	return reference < 1 ? 1 : (int)reference;
}

int mp_chain_utilization_on(const struct mp_chain *chain, int speed, int type)
{
	int total = 0, i;

	/* from the reference WCETs: @chain may come from another core */
	for (i = 0; i < chain->tasks.count; i++) {
		const struct mp_task *task = &chain->tasks.items[i];

		total += mp_utilization(mp_task_wcet_on(task, speed, type),
					task->period);
	}
	return total;
}

int mp_task_wcet_on_core(const struct mp_task *task,
			 const struct mp_core *core)
{
	int64_t wcet = mp_task_wcet_on(task, core->speed, core->type);

	/* round robin: each request waits at most one request of every
	 * other core of the controller */
	wcet += ((int64_t)task->accesses * core->access_stall + 999) / 1000;
	return wcet > 0x3fffffff ? 0x3fffffff : (int)wcet;
}

int mp_chain_utilization_on_core(const struct mp_chain *chain,
				 const struct mp_core *core)
{
	int total = 0, i;

	for (i = 0; i < chain->tasks.count; i++) {
		const struct mp_task *task = &chain->tasks.items[i];

		total += mp_utilization(mp_task_wcet_on_core(task, core),
					task->period);
	}
	return total;
}

void mp_chain_convert_to_core(struct mp_chain *chain,
			      const struct mp_core *core)
{
	int i;

	chain->utilization = 0;
	for (i = 0; i < chain->tasks.count; i++) {
		struct mp_task *task = &chain->tasks.items[i];

		if (!task->reference_wcet)
			task->reference_wcet = task->wcet;
		task->wcet = mp_task_wcet_on_core(task, core);
		task->utilization = mp_utilization(task->wcet, task->period);
		chain->utilization += task->utilization;
	}
}

unsigned mp_task_types(const struct mp_task *task)
{
	return task->types ? task->types : MP_ALL_TYPES;
}

int mp_chain_colocate(struct mp_chain *chain, int *task)
{
	int first, last, i;

	for (first = 0; first < chain->tasks.count; first = last + 1) {
		int controller = MP_CONTROLLER_ANY;
		unsigned allowed = MP_ALL_CONTROLLERS, types = MP_ALL_TYPES;

		for (last = first; last + 1 < chain->tasks.count &&
		     chain->tasks.items[last].colocate_next; last++)
			;
		if (last == first)
			continue;
		for (i = first; i <= last; i++) {
			const struct mp_task *t = &chain->tasks.items[i];

			if (t->controller != MP_CONTROLLER_ANY) {
				if (controller != MP_CONTROLLER_ANY &&
				    controller != t->controller)
					allowed = 0;
				controller = t->controller;
			}
			allowed &= mp_task_allowed(t);
			types &= mp_task_types(t);
		}
		if (!allowed || !types) {
			*task = first;
			return -EINVAL;
		}
		for (i = first; i <= last; i++) {
			struct mp_task *t = &chain->tasks.items[i];

			t->controller = controller;
			t->allowed = controller == MP_CONTROLLER_ANY &&
				     allowed != MP_ALL_CONTROLLERS ? allowed : 0;
			t->types = types == MP_ALL_TYPES ? 0 : types;
		}
	}
	return 0;
}

unsigned mp_chain_types(const struct mp_chain *chain)
{
	unsigned types = MP_ALL_TYPES;
	int i;

	for (i = 0; i < chain->tasks.count; i++)
		types &= mp_task_types(&chain->tasks.items[i]);
	return types;
}

unsigned mp_task_allowed(const struct mp_task *task)
{
	if (task->allowed)
		return task->allowed;
	return task->controller == MP_CONTROLLER_ANY ? MP_ALL_CONTROLLERS :
	       MP_CONTROLLER_BIT(task->controller);
}

unsigned mp_chain_allowed(const struct mp_chain *chain)
{
	unsigned allowed = MP_ALL_CONTROLLERS;
	int i;

	if (chain->allowed)
		return chain->allowed;
	if (chain->controller != MP_CONTROLLER_ANY)
		return MP_CONTROLLER_BIT(chain->controller);
	for (i = 0; i < chain->tasks.count; i++)
		allowed &= mp_task_allowed(&chain->tasks.items[i]);
	return allowed;
}
