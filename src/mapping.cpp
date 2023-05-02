#include "print.h"
#include "mapping.h"
#include "optimization.h"
#include "sched_analysis.h"

void generation(vector<struct bin> &v_bins, struct context &ctx)
{
        printf("+=====================================+\n");
        printf("| GENERATION                          |\n");
        printf("+=====================================+\n");

        for (int i = 0; i < ctx.bins_min; i++)
                add_bin(v_bins, ctx);
}

void allocation(vector<struct item> &v_itms, vector<struct bin> &v_bins, 
                struct context &ctx)
{
        clock_t start, end;
        if (ctx.prm.a == BFDU_F) {
                printf("+=====================================+\n");
                printf("| ALLOCATION BFDU_F                   |\n");
                printf("+=====================================+\n");
                start = clock();
                bfdu_f(v_itms, v_bins, ctx);
                end = clock();
                ctx.p.allo_time = ((float) (end - start)) / CLOCKS_PER_SEC;
        }

        if (ctx.prm.a == WFDU_F) {
                printf("+=====================================+\n");
                printf("| ALLOCATION WFDU_F                   |\n");
                printf("+=====================================+\n");
                start = clock();
                wfdu_f(v_itms, v_bins, ctx);
                end = clock();
                ctx.p.allo_time = ((float) (end - start)) / CLOCKS_PER_SEC;
        }
}

void schedulability_analysis(vector<struct bin> &v_bins, struct context &ctx)
{
        clock_t start, end;
        printf("+=====================================+\n");
        printf("| SCHEDULABILITY ANALYSIS             |\n");
        printf("+=====================================+\n");
        start = clock();
        sched_analysis(v_bins, ctx);
        end = clock();
        ctx.p.schd_time = ((float) (end - start)) / CLOCKS_PER_SEC;

        /*compute improvement */
        ctx.p.sched_imp_reas = ctx.p.sched_imp_reas + ctx.sched_ok_count;
}

void optimization(vector<struct bin> &v_bins, struct context &ctx)
{
        clock_t start, end;
        printf("+=====================================+\n");
        printf("| DISPLACEMENT OPTIMIZATION           |\n");
        printf("+=====================================+\n");
        start = clock();
        displacement(v_bins);
        end = clock();
        ctx.p.disp_time = ((float) (end - start)) / CLOCKS_PER_SEC;
        ctx.p.sched_rate_disp = sched_rate(v_bins, ctx);

        /*compute improvement */
        ctx.p.sched_imp_disp -= ctx.sched_ok_count;
        ctx.p.sched_imp_disp = ctx.p.sched_imp_disp + ctx.sched_ok_count;

        printf("+=====================================+\n");
        printf("| SWAPPING OPTIMIZATION               |\n");
        printf("+=====================================+\n");
        start = clock();
        swapping(v_bins);
        end = clock();
        ctx.p.swap_time = ((float) (end - start)) / CLOCKS_PER_SEC;
        ctx.p.sched_rate_swap = sched_rate(v_bins, ctx);

        /*compute improvement */
        ctx.p.sched_imp_swap -= ctx.sched_ok_count;
        ctx.p.sched_imp_swap = ctx.p.sched_imp_swap + ctx.sched_ok_count;
}
