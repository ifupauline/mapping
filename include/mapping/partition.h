/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MAPPING_PARTITION_H
#define MAPPING_PARTITION_H

#include "mapping/context.h"
#include "mapping/model.h"

/* margin kept below sigma for the LET task, ppm (1 %) */
#define MP_LET_MARGIN 10000

/**
 * mp_partition() - cut the task-chains into fragments
 * @ctx: ctx->params.sigma is the size limit; ctx->cuts, ctx->forced_cuts
 *	 and ctx->max_cuts are set (fragmentation rate FR = cuts / max_cuts)
 * @chains: the application on entry, its fragments on return, sorted by
 *	    decreasing utilization
 *
 * Each chain, in precedence order, is cut into contiguous fragments such
 * that a fragment is schedulable on its own, its tasks share the same
 * binding (all free, or all bound to the same controller, which the
 * fragment is then bound to), and, unless it is a single task, its utilization does not exceed
 * sigma minus a margin for the LET task. A task heavier than that limit
 * becomes a fragment by itself.
 * Return: 0, -ENOMEM or -EINVAL (@chains unchanged on error).
 */
int mp_partition(struct mp_ctx *ctx, struct mp_chain_array *chains);

#endif /* MAPPING_PARTITION_H */
