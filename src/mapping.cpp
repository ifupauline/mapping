#include "mapping.h"
#include "optimization.h"
#include "sched_analysis.h"

static void find_single_fit(vector<struct item> &v_itms, 
                vector<struct bin> &v_bins, struct context &ctx)
{
        clock_t start, end;

        start = clock();
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
        end = clock();
        ctx.redu_time += ((float) (end - start)) / CLOCKS_PER_SEC;
}

static void find_double_fit(vector<struct item> &v_itms, 
                vector<struct bin> &v_bins, struct context &ctx)
{
        clock_t start, end;

        start = clock();
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
        end = clock();
        ctx.redu_time += ((float) (end - start)) / CLOCKS_PER_SEC;
}

void add_bin(vector<struct bin> &v_bins, struct context &ctx)
{
        struct bin tmp_bin;
        tmp_bin.id = ctx.bins_count;
        tmp_bin.flag = -1;
        tmp_bin.cap_rem = ctx.prm.phi;
        v_bins.push_back(tmp_bin);
        ctx.bins_count++;
        printf("Bin %d Created\n\n", ctx.bins_count - 1);
}

void add_itm_to_bin(vector<struct bin> &v_bins, struct item &itm, int bin_id, 
                struct context &ctx)
{
        for (int i = 0; i < ctx.bins_count; i++) {
                if (v_bins[i].id == bin_id) {
                        if (v_bins[i].cap_rem < itm.size) {
                                printf("ERR Bin %d Overflow with itm.size %d\n", 
                                                v_bins[i].id, itm.size);
                                exit(0);
                        }
                        itm.is_allocated = YES;
                        v_bins[i].v_itms.push_back(itm);
                        v_bins[i].cap_rem -= itm.size;

                        if (itm.is_frag == NO) {
                                printf("Item %d added in Bin %d\n\n", 
                                                itm.id, v_bins[i].id);
                                return;

                        } else {
                                printf("Fragment %d added in Bin %d\n\n", 
                                                itm.id, v_bins[i].id);
                                return;
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

        find_single_fit(v_itms, v_bins, ctx);
        find_double_fit(v_itms, v_bins, ctx);
        printf("\n");
}

void allocation(vector<struct item> &v_itms, vector<struct bin> &v_bins, 
                struct context &ctx)
{
        if (ctx.prm.a == BFDU_F) {
                printf("+=====================================+\n");
                printf("| ALLOCATION BFDU_F                   |\n");
                printf("+=====================================+\n");
                bfdu_f(v_itms, v_bins, ctx);
        }

        if (ctx.prm.a == WFDU_F) {
                printf("+=====================================+\n");
                printf("| ALLOCATION WFDU_F                   |\n");
                printf("+=====================================+\n");
                wfdu_f(v_itms, v_bins, ctx);
        }
}

void worst_case_analysis(vector<struct bin> &v_bins, struct context &ctx)
{
        if (ctx.prm.a == BFDU_F) {
                printf("+=====================================+\n");
                printf("| WORST-CASE-ANALYSIS BFDU_F          |\n");
                printf("+=====================================+\n");
                priority_assignment(v_bins);
                sched_analysis(v_bins, ctx);
        }

        if (ctx.prm.a == WFDU_F) {
                printf("+=====================================+\n");
                printf("| WORST-CASE-ANALYSIS WFDU_F          |\n");
                printf("+=====================================+\n");
                priority_assignment(v_bins);
                sched_analysis(v_bins, ctx);
        }
}

void optimization(vector<struct bin> &v_bins, struct context &ctx)
{
        if (ctx.prm.a == BFDU_F) {
                printf("+=====================================+\n");
                printf("| OPTIMIZATION BFDU_F                 |\n");
                printf("+=====================================+\n");
                opti_priority(v_bins, ctx);
        }

        if (ctx.prm.a == WFDU_F) {
                printf("+=====================================+\n");
                printf("| OPTIMIZATION WFDU_F                 |\n");
                printf("+=====================================+\n");
                opti_priority(v_bins, ctx);
        }
}
