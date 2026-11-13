/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MAPPING_REPAIR_H
#define MAPPING_REPAIR_H

#include "mapping/context.h"
#include "mapping/model.h"

/*
 * mp_repair_deadlines() - move fragments so that more task-chains meet
 * their end-to-end deadline (see src/repair.c)
 *
 * Runs after displacement. Each kept move increments
 * ctx->deadline_repairs. Chains without a deadline (deadline 0) are only
 * repaired when they cross an overloaded network port.
 * Return: 0 or a negative errno.
 */
int mp_repair_deadlines(struct mp_ctx *ctx, struct mp_core_array *cores);

#endif /* MAPPING_REPAIR_H */
