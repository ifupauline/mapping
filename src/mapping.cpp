#include "print.h"
#include "mapping.h"
#include "generator.h"
#include "optimization.h"
#include "communication.h"
#include "sched_analysis.h"

void fragmentation(vector<struct item> &v_itms, struct context &ctx)
{
        clock_t start, end;

        printf("+=====================================+\n");
        printf("| PARTITIONING                        |\n");
        printf("+=====================================+\n");
        start = clock();
        partitioning(v_itms, ctx);
        end = clock();
        ctx.p.frag_time = ((float) (end - start)) / CLOCKS_PER_SEC;
}

void generation(vector<struct bin> &v_bins, struct context &ctx)
{
        printf("+=====================================+\n");
        printf("| GENERATION                          |\n");
        printf("+=====================================+\n");

        /* create 42 cores */
        for (int i = 0; i < 8; i++)
                add_bin_color(v_bins, RED, ctx);

        for (int i = 0; i < 4; i++) {
                add_bin_color(v_bins, BLUE, ctx);
                add_bin_color(v_bins, YELLOW, ctx);
                add_bin_color(v_bins, GREEN, ctx);
                add_bin_color(v_bins, CYAN, ctx);
                add_bin_color(v_bins, PURPLE, ctx);
        }
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

        if (ctx.prm.a == FFDU_F) {
                printf("+=====================================+\n");
                printf("| ALLOCATION FFDU_FF                  |\n");
                printf("+=====================================+\n");
                start = clock();
                frst_f(v_itms, v_bins, ctx);
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
        ctx.p.sched_rate_allo = sched_rate(v_bins, ctx);

        for (unsigned int i = 0; i < v_bins.size(); i++)
                is_task_same_v_tasks(v_bins[i]);
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
        ctx.p.sched_imp_disp -= ctx.p.sched_imp_allo;
        ctx.p.sched_imp_disp = ctx.p.sched_imp_disp + ctx.sched_ok_count;

        /* prepare next swamp imp */
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

void communication(vector<struct bin> &v_bins, vector<struct item> &v_itms, 
                vector<struct ecu> &v_ecus, struct context &ctx)
{
        clock_t start, end;
        printf("+=====================================+\n");
        printf("| COMMUNICATION                       |\n");
        printf("+=====================================+\n");
        start = clock();
        create_ecus(v_bins, v_ecus);
        comm_count(v_bins, v_itms, v_ecus, ctx);
        e2e_latency(v_bins, v_itms, ctx);
        end = clock();
        ctx.p.comm_time = ((float) (end - start)) / CLOCKS_PER_SEC;
}
