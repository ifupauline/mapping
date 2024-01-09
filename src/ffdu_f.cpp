#include "let.h"
#include "print.h"
#include "mapping.h"
#include "generator.h"

static int _find_ffdu_bin(vector<struct bin> &v_bins, struct item &itm,
                struct context &ctx, int &ffdu_load, int &ffdu_gcd, int color)
{
        int bin_id;

        bin_id = 0;

        for (unsigned int i = 0; i < v_bins.size(); i++) {
                if (v_bins[i].color != color && color != WHITE) 
                        continue;

                ffdu_load = check_if_fit_itm(v_bins[i], itm, ffdu_gcd);
                if (ffdu_load <= v_bins[i].phi) {
                        bin_id = v_bins[i].id;
                        return bin_id;
                }
        }
        return -1;
}

void ffdu_f(vector<struct item> &v_itms, vector<struct bin> &v_bins, 
                struct context &ctx)
{
        int n;
        int ret;
        int load;
        int gcd;
        int bin_id;
        int alloc_count;

        n = v_itms.size();

        sort_inc_itm_color(v_itms);

        /* STEP - 1, place all possible items in bins using ffdu */
        printf("\n<--------------------------------------->\n");
        printf("STEP 1, FFDU_F\n");
        printf("<--------------------------------------->\n");
        while (alloc_count != n) {
                alloc_count = 0;
                for (int i = 0; i < n; i++) {
                        ret = 0;
                        gcd = 0;
                        load = 0;
                        bin_id = 0;
                        if (v_itms[i].is_allocated == YES) 
                                continue;

                        /* find best bin to fit itm */
                        ret = _find_ffdu_bin(v_bins, v_itms[i], ctx, load, gcd, v_itms[i].color);

                        /* bin found add itm to it */
                        if (ret != -1) {
                                printf("First Bin to accomodate Item %d is Bin %d\n", 
                                                v_itms[i].id, ret);
                                bin_id = ret;
                                add_itm_to_v_bins(v_bins, v_itms[i], bin_id, ctx, 
                                                load, gcd);
                                v_itms[i].is_allocated = YES;
                                continue;

                                /* no bin was found */
                        } else {
                                printf("No Bin was found to accomodate Item %d idx: %d size: %d\n", 
                                                v_itms[i].id, v_itms[i].tc_idx, v_itms[i].size);

                                if (v_itms[i].color == WHITE)
                                        add_bin_color(v_bins, gen_rand(0, 5), ctx);
                                else
                                        add_bin_color(v_bins, v_itms[i].color, ctx);
                                ctx.cycl_count++;
                                continue;
                        }
                }
                /* count remaining item to be allocated */
                for (int i = 0; i < n; i++) {
                        if (v_itms[i].is_allocated == YES)
                                alloc_count++;
                }
        }
}
