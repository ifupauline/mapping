#include "augmentation.h"
#include "optimization.h"
#include "sched_analysis.h"

void converge(vector<struct bin> &v_bins, struct context &ctx)
{
        float rate;
        clock_t start, end;

        rate = 0.0;

        if (ctx.p.sched_rate_swap == 1)
                return;

        while (rate != PERCENT) {
                start = clock();
                add_bin(v_bins, ctx);
                end = clock();
                ctx.p.alloc_time += ((float) (end - start)) / CLOCKS_PER_SEC;

                start = clock();
                displacement(v_bins);
                end = clock();
                ctx.p.disp_time += ((float) (end - start)) / CLOCKS_PER_SEC;

                start = clock();
                swapping(v_bins);
                end = clock();
                ctx.p.swap_time += ((float) (end - start)) / CLOCKS_PER_SEC;

                rate = sched_rate(v_bins, ctx);
                rate = rate * PERCENT;
        }
}
