#include "mapping.h"

struct worst_cut {
        int id;
        int lf_size;
        int rf_size;
        int target_bin_lf;
        int target_bin_rf;
        int diff;
};

static vector<struct item> frags_wfdu_f;

static struct worst_cut cut = {0};

static int find_worst_bin(vector<struct bin> &lst_bins, struct item &itm,
                struct context &ctx)
{
        int worst_rem = -1;
        int bin_id = 0;
        int tmp = ctx.prm.phi;
        int is_found = NO;

        for (unsigned int i = 0; i < lst_bins.size(); i++) {
                if (itm.size <= lst_bins[i].cap_rem) {
                        tmp = lst_bins[i].cap_rem - itm.size;

                        if (tmp > worst_rem) {
                                worst_rem = tmp;
                                bin_id = lst_bins[i].id;
                        }
                        is_found = YES;
                        continue;
                }
                if (itm.size > ctx.prm.phi) 
                        is_found = -2;
        }
        if (is_found == YES) 
                return bin_id;

        else if (is_found == NO) 
                return -1;

        else if (is_found == -2) 
                return -2;

        return -1;
}

static int find_worst_cut(vector<struct bin> &lst_bins, struct item &itm,
                struct context &ctx)
{
        int l_diff;
        int r_diff;

        int l_worst_diff;
        int r_worst_diff;

        int l_val = 0;
        int r_val = 0;

        int is_l_val_found = NO;
        int is_r_val_found = NO;

        int tmp_max = 0;
        int is_worst_found = NO;
        struct worst_cut tmp_cut = {0};

        /* fragment cannot be fragmented */
        if (itm.is_frag == YES) 
                return -1;

        /* check for a cut */
        for (unsigned int i = 0; i < itm.lst_cuts.size(); i++) {
                is_l_val_found = NO;
                is_r_val_found = NO;

                l_worst_diff = -1;
                r_worst_diff = -1;

                l_diff = ctx.prm.phi;
                r_diff = ctx.prm.phi;

                l_val = itm.lst_cuts[i].c_pair.first;
                r_val = itm.lst_cuts[i].c_pair.second;

                tmp_cut = {0};

                for (unsigned int j = 0; j < lst_bins.size(); j++) {
                        /* check if fragment can be placed in a bin */
                        if (l_val <= lst_bins[j].cap_rem) {
                                is_l_val_found = YES;

                                /* compute the remaining */
                                l_diff = lst_bins[j].cap_rem - l_val; 

                                /* save the bin for worst fit */
                                if (l_diff > l_worst_diff) {
                                        l_worst_diff = l_diff;
                                        tmp_cut.id = itm.lst_cuts[i].id;
                                        tmp_cut.target_bin_lf = lst_bins[j].id;
                                        tmp_cut.lf_size = l_val;
                                }

                        } else {
                                /* don't bother check other frag if no match */
                                is_l_val_found = NO;
                                continue;
                        }
                }

                /* right value */
                for (unsigned int j = 0; j < lst_bins.size(); j++) {
                        if (lst_bins[j].id == tmp_cut.target_bin_lf) 
                                continue;

                        /* check if fragment can be placed in a bin */
                        if (r_val <= lst_bins[j].cap_rem) {
                                is_r_val_found = YES;

                                /* compute the remaining */
                                r_diff = lst_bins[j].cap_rem - r_val;

                                /* save the bin for worst fit */
                                if (r_diff > r_worst_diff) {
                                        r_worst_diff = r_diff;
                                        tmp_cut.id = itm.lst_cuts[i].id;
                                        tmp_cut.target_bin_rf = lst_bins[j].id;
                                        tmp_cut.rf_size = r_val;
                                }
                        }
                }

                /* if cut found find the one with the worst diff */
                if (is_l_val_found == YES && is_r_val_found == YES) {
                        tmp_cut.diff = l_worst_diff + r_worst_diff;

                        /* 
                         * must be >= or else it will never find a cut for itm
                         * with a size twice the size of k.
                         */
                        if (ctx.prm.phi <= ctx.prm.s / 2) {
                                if (tmp_cut.diff >= tmp_max) {
                                        tmp_max = tmp_cut.diff;
                                        cut = tmp_cut;
                                        is_worst_found = YES;
                                }

                        } else {
                                if (tmp_cut.diff > tmp_max) {
                                        tmp_max = tmp_cut.diff;
                                        cut = tmp_cut;
                                        is_worst_found = YES;
                                }
                        }
                }
        }

        if (is_worst_found == YES)
                return YES;

        else 
                return NO;
}

static void wff(vector<struct item> &lst_itms, vector<struct bin> &lst_bins, 
                struct item &itm, struct worst_cut &cut, struct context &ctx)
{
        struct item itm_lf;
        struct item itm_rf;

        /* creates first itm with left fragment*/
        ctx.itms_count++;
        itm_lf.id = ctx.itms_count;
        itm_lf.is_allocated = YES;
        itm_lf.is_frag = YES;
        itm_lf.is_fragmented = NO;
        itm_lf.nbr_cut = 0;
        itm_lf.size = cut.lf_size;

        /* creates second itm with right fragment*/
        ctx.itms_count++;
        itm_rf.id = ctx.itms_count;
        itm_rf.is_allocated = YES;
        itm_rf.is_frag = YES;
        itm_rf.is_fragmented = NO;
        itm_rf.nbr_cut = 0;
        itm_rf.size = cut.rf_size;

        printf("\nLeft Fragment %d has been created from Item %d with size %d\n", 
                        itm_lf.id, itm.id, itm_lf.size);
        printf("Right Fragment %d has been created from Item %d with size %d\n\n", 
                        itm_rf.id, itm.id, itm_rf.size);

        if (itm_lf.size == 0 || itm_rf.size == 0) {
                printf("ERR Fragment size = 0!\n");
                exit(0);
        }

        /* store fragments */
        frags_wfdu_f.push_back(itm_lf);
        frags_wfdu_f.push_back(itm_rf);

        /* add fragments to their respective bins */
        add_itm_to_bin(lst_bins, itm_lf, cut.target_bin_lf, ctx);
        add_itm_to_bin(lst_bins, itm_rf, cut.target_bin_rf, ctx);
}

void wfdu_f(vector<struct item> &lst_itms, vector<struct bin> &lst_bins, 
                struct context &ctx)
{
        int ret;
        int alloc_count = 0;
        clock_t start, end;

        /* STEP - 1, place all possible items in bins using WFDU */
        printf("\n<--------------------------------------->\n");
        printf("STEP 1, WFDU\n");
        printf("<--------------------------------------->\n");
        start = clock();
        for (int i = 0; i < ctx.prm.n; i++) {
                if (lst_itms[i].is_allocated == YES) 
                        continue;

                /* find best bin to fit itm */
                ret = find_worst_bin(lst_bins, lst_itms[i], ctx);

                /* bin found add itm to it */
                if (ret > -1) {
                        printf("Worst Bin to accomodate Item %d is Bin %d\n", 
                                        lst_itms[i].id, ret);
                        add_itm_to_bin(lst_bins, lst_itms[i], ret, ctx);
                        lst_itms[i].is_allocated = YES;

                        /* no bin was found */
                } else if (ret == -1) {
                        printf("No Bin was found to accomodate Item %d\n", 
                                        lst_itms[i].id);

                        /* size bigger than PHI */
                } else if (ret == -2) {
                        printf("Item %d of size %d bigger than PHI\n", 
                                        lst_itms[i].id, lst_itms[i].size);
                }
        }

        end = clock();
        ctx.alloc_time = ((float) (end - start)) / CLOCKS_PER_SEC;

        /* 
         * STEP - 2, try to place remaining items using fragmentation 
         * with pair 
         */
        while (alloc_count != ctx.prm.n) {
                alloc_count = 0;
                printf("\n<--------------------------------------->\n");
                printf("STEP 2, WORST-FIT FRAGMENTATION\n");
                printf("<--------------------------------------->\n");
                start = clock();
                for (int i = 0; i < ctx.prm.n; i++) {
                        if (lst_itms[i].is_allocated == NO) {
                                ret = find_worst_cut(lst_bins, lst_itms[i], ctx);
                                if (ret == YES) {
                                        printf("Found Cut %d for Item %d\n", 
                                                        cut.id, lst_itms[i].id);
                                        wff(lst_itms, lst_bins, lst_itms[i], 
                                                        cut, ctx);
                                        lst_itms[i].is_allocated = YES;
                                        lst_itms[i].is_fragmented = YES;
                                        continue;
                                } 
                                if (ret == NO) {
                                        add_bin(lst_bins, ctx);
                                        ctx.cycl_count++;
                                        continue;
                                }
                        }
                }

                end = clock();
                ctx.frag_time += ((float) (end - start)) / CLOCKS_PER_SEC;

                for (int i = 0; i < ctx.prm.n; i++) {
                        if (lst_itms[i].is_allocated == YES)
                                alloc_count++;
                }
        }
}

vector<struct item> *get_frags_wfdu_f(void)
{
        return &frags_wfdu_f;
}
