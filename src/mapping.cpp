#include "optimization.h"
#include "augmentation.h"
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
        if (ctx.prm.a == BFDU_F) {
                printf("+=====================================+\n");
                printf("| ALLOCATION BFDU_F                   |\n");
                printf("+=====================================+\n");

                clock_t start, end;
                start = clock();
                bfdu_f(v_itms, v_bins, ctx);
                end = clock();
                ctx.p.alloc_time = ((float) (end - start)) / CLOCKS_PER_SEC;
        }

        if (ctx.prm.a == WFDU_F) {
                printf("+=====================================+\n");
                printf("| ALLOCATION WFDU_F                   |\n");
                printf("+=====================================+\n");

                clock_t start, end;
                start = clock();
                wfdu_f(v_itms, v_bins, ctx);
                end = clock();
                ctx.p.alloc_time = ((float) (end - start)) / CLOCKS_PER_SEC;
        }
}

void schedulability_analysis(vector<struct bin> &v_bins, struct context &ctx)
{
        printf("+=====================================+\n");
        printf("| SCHEDULABILITY ANALYSIS             |\n");
        printf("+=====================================+\n");
        clock_t start, end;
        start = clock();
        wcrt_v_bins(v_bins, ctx);
        end = clock();
        ctx.p.wcrt_time = ((float) (end - start)) / CLOCKS_PER_SEC;
        ctx.p.sched_rate_allo = sched_rate(v_bins, ctx);

        /* init var for priority optimization */
        ctx.p.sched_imp_prio -= ctx.sched_ok_count;
}

void optimization(vector<struct bin> &v_bins, struct context &ctx)
{
        printf("+=====================================+\n");
        printf("| REASSIGNMENT OPTIMIZATION           |\n");
        printf("+=====================================+\n");
        clock_t start, end;
        start = clock();
        reassignment(v_bins);
        end = clock();
        ctx.p.reass_time = ((float) (end - start)) / CLOCKS_PER_SEC;
        ctx.p.sched_rate_prio = sched_rate(v_bins, ctx);

        /*compute improvement */
        ctx.p.sched_imp_prio = ctx.p.sched_imp_prio + ctx.sched_ok_count;

        /* init var for displacement */
        ctx.p.sched_imp_disp -= ctx.sched_ok_count;

        printf("+=====================================+\n");
        printf("| DISPLACEMENT OPTIMIZATION           |\n");
        printf("+=====================================+\n");
        start = clock();
        displacement(v_bins);
        end = clock();
        ctx.p.disp_time = ((float) (end - start)) / CLOCKS_PER_SEC;
        ctx.p.sched_rate_disp = sched_rate(v_bins, ctx);

        /*compute improvement */
        ctx.p.sched_imp_disp = ctx.p.sched_imp_disp + ctx.sched_ok_count;

        /* init var for swapping */
        ctx.p.sched_imp_swap -= ctx.sched_ok_count;

        printf("+=====================================+\n");
        printf("| SWAPPING OPTIMIZATION               |\n");
        printf("+=====================================+\n");
        start = clock();
        swapping(v_bins);
        end = clock();
        ctx.p.swap_time = ((float) (end - start)) / CLOCKS_PER_SEC;
        ctx.p.sched_rate_swap = sched_rate(v_bins, ctx);

        /*compute improvement */
        ctx.p.sched_imp_swap = ctx.p.sched_imp_swap + ctx.sched_ok_count;
}

void augmentation(vector<struct bin> &v_bins, struct context &ctx)
{
        printf("+=====================================+\n");
        printf("| AUGMENTATION                        |\n");
        printf("+=====================================+\n");
        converge(v_bins, ctx);
        ctx.p.sched_rate_augm = sched_rate(v_bins, ctx);
}
