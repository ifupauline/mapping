#include "augmentation.h"
#include "optimization.h"
#include "sched_analysis.h"

void converge(vector<struct bin> &v_bins, struct context &ctx)
{
        float rate;

        rate = 0.0;

        if (ctx.p.sched_rate_swap == 1)
                return;

        while (rate != PERCENT) {
                add_bin(v_bins, ctx);
                displacement(v_bins);
                swapping(v_bins);
                rate = sched_rate(v_bins, ctx);
                rate = rate * PERCENT;
        }
}
