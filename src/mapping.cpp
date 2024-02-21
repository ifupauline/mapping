#include "print.h"
#include "mapping.h"
#include "generator.h"
#include "placement.h"
#include "sched_analysis.h"

int STATE = OK;

void partitioning(vector<struct tc> &v_tcs, struct context &ctx)
{
        clock_t start, end;

        printf("+=====================================+\n");
        printf("| PARTITIONING                        |\n");
        printf("+=====================================+\n");
        start = clock();
        cut(v_tcs, ctx);
        end = clock();
        ctx.p.part_time = ((float) (end - start)) / CLOCKS_PER_SEC;
}

void allocation(vector<struct tc> &v_tcs, vector<struct core> &v_cores, 
                struct context &ctx)
{
        clock_t start, end;

        if (ctx.prm.a == BFDU) {
                printf("+=====================================+\n");
                printf("| ALLOCATION BFDU                     |\n");
                printf("+=====================================+\n");
                start = clock();
                bfdu(v_tcs, v_cores, ctx);
                end = clock();
                ctx.p.allo_time = ((float) (end - start)) / CLOCKS_PER_SEC;
        }

        if (ctx.prm.a == WFDU) {
                printf("+=====================================+\n");
                printf("| ALLOCATION WFDU                     |\n");
                printf("+=====================================+\n");
                start = clock();
                wfdu(v_tcs, v_cores, ctx);
                end = clock();
                ctx.p.allo_time = ((float) (end - start)) / CLOCKS_PER_SEC;
        }

        if (ctx.prm.a == FFDU) {
                printf("+=====================================+\n");
                printf("| ALLOCATION FFDU                     |\n");
                printf("+=====================================+\n");
                start = clock();
                ffdu(v_tcs, v_cores, ctx);
                end = clock();
                ctx.p.allo_time = ((float) (end - start)) / CLOCKS_PER_SEC;
        }
}

void schedulability_analysis(vector<struct core> &v_cores, struct context &ctx)
{
        clock_t start, end;
        printf("+=====================================+\n");
        printf("| SCHEDULABILITY ANALYSIS             |\n");
        printf("+=====================================+\n");
        start = clock();
        sched_analysis(v_cores, ctx);
        end = clock();
        ctx.p.schd_time = ((float) (end - start)) / CLOCKS_PER_SEC;
        ctx.p.sched_rate_allo = sched_rate(v_cores, ctx);
}

void placement(vector<struct core> &v_cores, struct context &ctx)
{
        clock_t start, end;
        printf("+=====================================+\n");
        printf("| PLACEMENT - SWAPPING                |\n");
        printf("+=====================================+\n");
        start = clock();
        swapping(v_cores);
        end = clock();
        ctx.p.swap_time = ((float) (end - start)) / CLOCKS_PER_SEC;
        ctx.p.sched_rate_swap = sched_rate(v_cores, ctx);

        /*compute improvement */
        ctx.p.sched_imp_swap -= ctx.p.sched_imp_allo;
        ctx.p.sched_imp_swap = ctx.p.sched_imp_swap + ctx.sched_ok_count;

        /* prepare next swap imp */
        ctx.p.sched_imp_disp -= ctx.sched_ok_count;

        printf("+=====================================+\n");
        printf("| PLACEMENT - DISPLACEMENT            |\n");
        printf("+=====================================+\n");
        start = clock();
        displacement(v_cores);
        end = clock();
        ctx.p.disp_time = ((float) (end - start)) / CLOCKS_PER_SEC;
        ctx.p.sched_rate_disp = sched_rate(v_cores, ctx);

        /*compute improvement */
        ctx.p.sched_imp_disp = ctx.p.sched_imp_disp + ctx.sched_ok_count;
}
