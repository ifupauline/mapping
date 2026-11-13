/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MAPPING_LET_H
#define MAPPING_LET_H

#include "mapping/model.h"

/*
 * LET mechanism. Every core runs one LET task that copies data between
 * local and global memory (or the network interface): highest priority,
 * period equal to the GCD of the periods of the tasks on the core, WCET
 * core->let_wcet, and nothing at all on a core without tasks.
 */

/*
 * Default WCET of a LET task on the reference core, microseconds. Biondi
 * et al. (2018) measured LET copies of 35.6 us on average (164 us at most)
 * on an automotive microcontroller; the zone controllers are slower, the
 * PCU cores faster, so 50 us on the reference core.
 */
#define MP_LET_WCET 50

/* index of the LET chain in core->chains, or -1 if the core has none */
int mp_let_chain_index(const struct mp_core *core);

/*
 * mp_let_update() - set the LET task of @core for a task period GCD
 * @period_gcd: GCD of the periods of the application tasks of the core,
 *		0 when the core has none (the LET task then costs nothing)
 *
 * Return: 0 or -EINVAL if the core has no LET chain.
 */
int mp_let_update(struct mp_core *core, int period_gcd);

/* utilization of a LET task of WCET @let_wcet and period @period_gcd,
 * ppm (0 without period) */
int mp_let_utilization(int let_wcet, int period_gcd);

/*
 * mp_core_update_load() - recompute core->load (all chains, LET included)
 * and core->load_remaining; returns the load.
 */
int mp_core_update_load(struct mp_core *core);

/*
 * mp_core_load_with_chain() - load of @core if @chain were added to it
 * @period_gcd: set to the GCD of the task periods after the addition, the
 *		new LET period
 *
 * The chain is counted at the speed of the core and the LET task is
 * resized for the new GCD.
 */
int mp_core_load_with_chain(const struct mp_core *core,
			    const struct mp_chain *chain, int *period_gcd);

/* greatest common divisor (Euclid); mp_gcd(0, x) = x */
int mp_gcd(int a, int b);

/* GCD of the periods of the application tasks of @core, 0 if none */
int mp_core_period_gcd(const struct mp_core *core);

#endif /* MAPPING_LET_H */
