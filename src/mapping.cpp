#include "optimization.h"
#include "sched_analysis.h"

static void _find_single_fit(vector<struct item> &v_itms, 
                vector<struct bin> &v_bins, struct context &ctx)
{
        for (int i = 0; i < ctx.prm.n; i++) {
                if (v_itms[i].size == ctx.prm.phi) {
                        for (int j = 0; j < ctx.bins_count; j++) {
                                if (v_bins[j].cap_rem == ctx.prm.phi) {
                                        add_itm_to_bin(v_bins, 
                                                        v_itms[i], 
                                                        v_bins[j].id, ctx);
                                        break;
                                }
                        }
                }
        }
}

static void _find_double_fit(vector<struct item> &v_itms, 
                vector<struct bin> &v_bins, struct context &ctx)
{
        for (int i = 0; i < ctx.prm.n - 1; i++)
        {
                for (int j = i + 1; j < ctx.prm.n; j++)
                {
                        if (v_itms[i].is_allocated == YES || 
                                        v_itms[j].is_allocated == YES) 
                                continue;

                        if (v_itms[i].size + v_itms[j].size == ctx.prm.phi)
                        {
                                for (int k = 0; k < ctx.bins_count; k++) {
                                        if (v_bins[k].cap_rem == ctx.prm.phi) {
                                                add_itm_to_bin(v_bins, 
                                                                v_itms[i], 
                                                                v_bins[k].id, ctx);
                                                add_itm_to_bin(v_bins, 
                                                                v_itms[j],  
                                                                v_bins[k].id, ctx);
                                                break;
                                        }
                                }
                        }
                }
        }
}

void generation(vector<struct bin> &v_bins, struct context &ctx)
{
        if (ctx.prm.a == BFDU_F) {
                printf("+=====================================+\n");
                printf("| GENERATION BFDU_F                   |\n");
                printf("+=====================================+\n");
        }

        if (ctx.prm.a == WFDU_F) {
                printf("+=====================================+\n");
                printf("| GENERATION WFDU_F                   |\n");
                printf("+=====================================+\n");
        }
        for (int i = 0; i < ctx.bins_min; i++)
                add_bin(v_bins, ctx);
}

void reduction(vector<struct item> &v_itms, vector<struct bin> &v_bins, 
                struct context &ctx)
{
        if (ctx.prm.a == BFDU_F) {
                printf("+=====================================+\n");
                printf("| REDUCTION BFDU_F                    |\n");
                printf("+=====================================+\n");
        }

        if (ctx.prm.a == WFDU_F) {
                printf("+=====================================+\n");
                printf("| REDUCTION WFDU_F                    |\n");
                printf("+=====================================+\n");
        }

        clock_t start, end;
        start = clock();
        _find_single_fit(v_itms, v_bins, ctx);
        _find_double_fit(v_itms, v_bins, ctx);
        end = clock();
        ctx.p.redu_time += ((float) (end - start)) / CLOCKS_PER_SEC;
        printf("\n");
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
        if (ctx.prm.a == BFDU_F) {
                printf("+=====================================+\n");
                printf("| SCHEDULABILITY ANALYSIS BFDU_F      |\n");
                printf("+=====================================+\n");

                clock_t start, end;
                start = clock();
                sched_analysis(v_bins, ctx);
                end = clock();
                ctx.p.wca_time = ((float) (end - start)) / CLOCKS_PER_SEC;

                ctx.p.sched_rate_bef = sched_rate(v_bins, ctx);
                ctx.p.sched_imp_prio = ctx.p.sched_imp_prio - ctx.sched_ok_count;
        }

        if (ctx.prm.a == WFDU_F) {
                printf("+=====================================+\n");
                printf("| SCHEDULABILITY ANALYSIS WFDU_F      |\n");
                printf("+=====================================+\n");

                clock_t start, end;
                start = clock();
                sched_analysis(v_bins, ctx);
                end = clock();
                ctx.p.wca_time = ((float) (end - start)) / CLOCKS_PER_SEC;

                ctx.p.sched_rate_bef = sched_rate(v_bins, ctx);
                ctx.p.sched_imp_prio = ctx.p.sched_imp_prio - ctx.sched_ok_count;
        }
}

void optimization(vector<struct bin> &v_bins, struct context &ctx)
{
        printf("+=====================================+\n");
        printf("| PRIORITY ASSIGNMENT OPTIMIZATION    |\n");
        printf("+=====================================+\n");

        clock_t start, end;
        start = clock();
        reassignment(v_bins);
        end = clock();
        ctx.p.reass_time = ((float) (end - start)) / CLOCKS_PER_SEC;
        ctx.p.sched_rate_prio = sched_rate(v_bins, ctx);
        ctx.p.sched_imp_prio = ctx.p.sched_imp_prio + ctx.sched_ok_count;

        ctx.p.sched_imp_disp = ctx.p.sched_imp_disp - ctx.sched_ok_count;

        printf("+=====================================+\n");
        printf("| DISPLACEMENT OPTIMIZATION           |\n");
        printf("+=====================================+\n");

        start = clock();
        displacement(v_bins);
        end = clock();
        ctx.p.disp_time = ((float) (end - start)) / CLOCKS_PER_SEC;

        ctx.p.sched_rate_disp = sched_rate(v_bins, ctx);
        ctx.p.sched_imp_disp = ctx.p.sched_imp_disp + ctx.sched_ok_count;
}
