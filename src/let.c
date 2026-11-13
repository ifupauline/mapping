// SPDX-License-Identifier: Apache-2.0
/* LET task sizing and core loads (see let.h). */
#include <errno.h>

#include "mapping/let.h"

int mp_let_chain_index(const struct mp_core *core)
{
	int i;

	for (i = 0; i < core->chains.count; i++)
		if (core->chains.items[i].is_let)
			return i;
	return -1;
}

int mp_let_utilization(int let_wcet, int period_gcd)
{
	if (!period_gcd)
		return 0;
	return mp_utilization(let_wcet, period_gcd);
}

int mp_let_update(struct mp_core *core, int period_gcd)
{
	int index = mp_let_chain_index(core);
	struct mp_chain *let_chain;
	struct mp_task *let_task;

	if (index < 0 || !core->chains.items[index].tasks.count)
		return -EINVAL;
	let_chain = &core->chains.items[index];
	let_task = &let_chain->tasks.items[0];

	if (period_gcd) {
		let_task->wcet = core->let_wcet;
		let_task->period = period_gcd;
	} else {
		/* no task on the core: no communication to perform */
		let_task->wcet = 0;
		let_task->period = 0;
	}
	let_task->utilization = period_gcd ?
		mp_let_utilization(core->let_wcet, period_gcd) : 0;
	let_task->is_let = true;
	let_task->priority = 1;		/* above every application task */

	let_chain->utilization = let_task->utilization;
	let_chain->period_gcd = period_gcd;
	return 0;
}

int mp_core_update_load(struct mp_core *core)
{
	int i;

	core->load = 0;
	for (i = 0; i < core->chains.count; i++)
		core->load += core->chains.items[i].utilization;
	core->load_remaining = core->fill_limit - core->load;
	return core->load;
}

int mp_gcd(int a, int b)
{
	while (b) {
		int remainder = a % b;

		a = b;
		b = remainder;
	}
	return a;
}

int mp_core_load_with_chain(const struct mp_core *core,
			    const struct mp_chain *chain, int *period_gcd)
{
	int load = 0, gcd = 0, i, j;

	/* GCD of the new chain's periods and of every application task */
	for (j = 0; j < chain->tasks.count; j++)
		gcd = mp_gcd(gcd, chain->tasks.items[j].period);
	for (i = 0; i < core->chains.count; i++) {
		const struct mp_chain *other = &core->chains.items[i];

		if (other->is_let)
			continue;
		for (j = 0; j < other->tasks.count; j++)
			gcd = mp_gcd(gcd, other->tasks.items[j].period);
		load += other->utilization;
	}
	*period_gcd = gcd;

	/* chains already there, the resized LET task and the new chain */
	/* the new chain at the speed of this core */
	return mp_chain_utilization_on_core(chain, core) + load +
	       mp_let_utilization(core->let_wcet, gcd);
}

int mp_core_period_gcd(const struct mp_core *core)
{
	int gcd = 0, i, j;

	for (i = 0; i < core->chains.count; i++) {
		const struct mp_chain *chain = &core->chains.items[i];

		if (chain->is_let)
			continue;
		for (j = 0; j < chain->tasks.count; j++)
			gcd = mp_gcd(gcd, chain->tasks.items[j].period);
	}
	return gcd;
}
