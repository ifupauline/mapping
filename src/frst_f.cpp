#include "let.h"
#include "mapping.h"

static int _find_frst_bin(vector<struct bin> &v_bins, struct item &itm,
                struct context &ctx, int &frst_load, int &frst_gcd)
{
        int bin_id;

        bin_id = 0;

        for (unsigned int i = 0; i < v_bins.size(); i++) {
                frst_load = check_if_fit_itm(v_bins[i], itm, frst_gcd);
                if (frst_load <= v_bins[i].phi) {
                        bin_id = v_bins[i].id;
                        return bin_id;
                }
        }
        return -1;
}

void frst_f(vector<struct item> &v_itms, vector<struct bin> &v_bins, 
                struct context &ctx)
{
        int n;
        int ret;
        int load;
        int gcd;
        int bin_id;
        int alloc_count;

        n = v_itms.size();

        /* STEP - 1, place all possible items in bins using FIRST */
        printf("\n<--------------------------------------->\n");
        printf("STEP 1, FIRST_F\n");
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
                        ret = _find_frst_bin(v_bins, v_itms[i], ctx, load, gcd);

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
                                printf("No Bin was found to accomodate Item %d size: %d\n", 
                                                v_itms[i].id, v_itms[i].size);
                                add_bin(v_bins, ctx);
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
