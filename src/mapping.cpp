#include "../include/mapping.h"

static void find_single_fit(vector<struct item> &lst_itms, 
                vector<struct bin> &lst_bins, struct context &ctx)
{
        clock_t start, end;

        start = clock();
        for (int i = 0; i < ctx.prm.n; i++) {
                if (lst_itms[i].size == ctx.prm.phi) {
                        for (int j = 0; j < ctx.bins_count; j++) {
                                if (lst_bins[j].cap_rem == ctx.prm.phi) {
                                        add_itm_to_bin(lst_bins, 
                                                        lst_itms[i], 
                                                        lst_bins[j].id, ctx);
                                        break;
                                }
                        }
                }
        }
        end = clock();
        ctx.redu_time += ((float) (end - start)) / CLOCKS_PER_SEC;
}

static void find_double_fit(vector<struct item> &lst_itms, 
                vector<struct bin> &lst_bins, struct context &ctx)
{
        clock_t start, end;

        start = clock();
        for (int i = 0; i < ctx.prm.n - 1; i++)
        {
                for (int j = i + 1; j < ctx.prm.n; j++)
                {
                        if (lst_itms[i].is_allocated == YES || 
                                        lst_itms[j].is_allocated == YES) 
                                continue;

                        if (lst_itms[i].size + lst_itms[j].size == ctx.prm.phi)
                        {

                                for (int k = 0; k < ctx.bins_count; k++) {
                                        if (lst_bins[k].cap_rem == ctx.prm.phi) {
                                                add_itm_to_bin(lst_bins, 
                                                                lst_itms[i], 
                                                                lst_bins[k].id, ctx);
                                                add_itm_to_bin(lst_bins, 
                                                                lst_itms[j],  
                                                                lst_bins[k].id, ctx);
                                                break;
                                        }
                                }
                        }
                }
        }
        end = clock();
        ctx.redu_time += ((float) (end - start)) / CLOCKS_PER_SEC;
}

void add_bin(vector<struct bin> &lst_bins, struct context &ctx)
{
        struct bin tmp_bin;
        tmp_bin.id = ctx.bins_count;
        tmp_bin.cap_rem = ctx.prm.phi;
        lst_bins.push_back(tmp_bin);
        ctx.bins_count++;
        printf("Bin %d Created\n", ctx.bins_count - 1);
}

void add_itm_to_bin(vector<struct bin> &lst_bins, struct item &itm, int bin_id, 
                struct context &ctx)
{
        for (int i = 0; i < ctx.bins_count; i++) {
                if (lst_bins[i].id == bin_id) {
                        if (lst_bins[i].cap_rem < itm.size) {
                                printf("ERR Bin %d Overflow with itm.size %d\n", 
                                                lst_bins[i].id, itm.size);
                                exit(0);
                        }
                        itm.is_allocated = YES;
                        lst_bins[i].vc_itms.push_back(itm);
                        lst_bins[i].cap_rem -= itm.size;

                        if (itm.is_frag == NO) {
                                printf("Item %d added in Bin %d\n\n", 
                                                itm.id, lst_bins[i].id);
                                return;

                        } else {
                                printf("Fragment %d added in Bin %d\n\n", 
                                                itm.id, lst_bins[i].id);
                                return;
                        }
                }
        }
}

void generation(vector<struct bin> &lst_bins, struct context &ctx)
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
                add_bin(lst_bins, ctx);
}

void reduction(vector<struct item> &lst_itms, vector<struct bin> &lst_bins, 
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

        find_single_fit(lst_itms, lst_bins, ctx);
        find_double_fit(lst_itms, lst_bins, ctx);
        printf("\n");
}

void allocation(vector<struct item> &lst_itms, vector<struct bin> &lst_bins, 
                struct context &ctx)
{
        if (ctx.prm.a == BFDU_F) {
                printf("+=====================================+\n");
                printf("| ALLOCATION BFDU_F                   |\n");
                printf("+=====================================+\n");
                bfdu_f(lst_itms, lst_bins, ctx);
        }

        if (ctx.prm.a == WFDU_F) {
                printf("+=====================================+\n");
                printf("| ALLOCATION WFDU_F                   |\n");
                printf("+=====================================+\n");
                wfdu_f(lst_itms, lst_bins, ctx);
        }
}
