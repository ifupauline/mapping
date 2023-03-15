#include "mapping.h"

struct best_cut {
        int id;
        int lf_size;
        int rf_size;
        int target_bin_lf;
        int target_bin_rf;
        int diff;
        struct cut c;
};

static int frag_id_count = 1;

static vector<struct item> frags_bfdu_f;

static struct best_cut cut = {0};

static int _find_best_bin(vector<struct bin> &v_bins, struct item &itm,
                struct context &ctx)
{
        int best_rem = ctx.prm.phi;
        int bin_id;
        int tmp;
        int is_found;

        bin_id = 0;
        tmp = 0;
        is_found = NO;
        best_rem = ctx.prm.phi;

        for (unsigned int i = 0; i < v_bins.size(); i++) {
                if (itm.size <= v_bins[i].cap_rem) {
                        tmp = v_bins[i].cap_rem - itm.size;

                        if (tmp < best_rem) {
                                best_rem = tmp;
                                bin_id = v_bins[i].id;
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

static int _find_best_cut(vector<struct bin> &v_bins, struct item &itm,
                struct context &ctx)
{
        int l_diff;
        int r_diff;

        int l_best_diff;
        int r_best_diff;

        int l_val;
        int r_val;

        int is_l_val_found;
        int is_r_val_found;

        int tmp_min;
        int is_best_found;
        struct best_cut tmp_cut;

        l_val = 0;
        r_val = 0;
        is_l_val_found = NO;
        is_r_val_found = NO;
        tmp_min = ctx.prm.phi;
        is_best_found = NO;
        tmp_cut = {0};

        /* fragment cannot be fragmented */
        if (itm.is_frag == YES) 
                return -1;

        /* check for a cut */
        for (unsigned int i = 0; i < itm.tc.v_cuts.size(); i++) {
                is_l_val_found = NO;
                is_r_val_found = NO;

                l_best_diff = ctx.prm.phi;
                r_best_diff = ctx.prm.phi;

                l_diff = 0;
                r_diff = 0;

                l_val = itm.tc.v_cuts[i].c_pair.first;
                r_val = itm.tc.v_cuts[i].c_pair.second;

                tmp_cut = {0};

                for (unsigned int j = 0; j < v_bins.size(); j++) {
                        /* check if fragment can be placed in a bin */
                        if (l_val <= v_bins[j].cap_rem) {
                                is_l_val_found = YES;

                                /* compute the remaining */
                                l_diff = v_bins[j].cap_rem - l_val; 

                                /* save the bin for best fit */
                                if (l_diff < l_best_diff) {
                                        l_best_diff = l_diff;
                                        tmp_cut.id = itm.tc.v_cuts[i].id;
                                        tmp_cut.c.v_tasks_lf = itm.tc.v_cuts[i].v_tasks_lf;
                                        tmp_cut.target_bin_lf = v_bins[j].id;
                                        tmp_cut.lf_size = l_val;
                                }

                        } else {
                                /* don't bother check other frag if no match */
                                is_l_val_found = NO;
                                continue;
                        }
                }

                /* right value */
                for (unsigned int j = 0; j < v_bins.size(); j++) {
                        if (v_bins[j].id == tmp_cut.target_bin_lf) 
                                continue;

                        /* check if fragment can be placed in a bin */
                        if (r_val <= v_bins[j].cap_rem) {
                                is_r_val_found = YES;

                                /* compute the remaining */
                                r_diff = v_bins[j].cap_rem - r_val;

                                /* save the bin for best fit */
                                if (r_diff < r_best_diff) {
                                        r_best_diff = r_diff;
                                        tmp_cut.id = itm.tc.v_cuts[i].id;
                                        tmp_cut.c.v_tasks_rf = itm.tc.v_cuts[i].v_tasks_rf;
                                        tmp_cut.target_bin_rf = v_bins[j].id;
                                        tmp_cut.rf_size = r_val;
                                }
                        }
                }

                /* if cut found find the one with the best diff */
                if (is_l_val_found == YES && is_r_val_found == YES) {
                        tmp_cut.diff = l_best_diff + r_best_diff;

                        if (tmp_cut.diff < tmp_min) {
                                tmp_min = tmp_cut.diff;
                                cut = tmp_cut;
                                is_best_found = YES;
                        }
                }
        }

        if (is_best_found == YES)
                return YES;
        else 
                return NO;
}

static void _bff(vector<struct item> &v_itms, vector<struct bin> &v_bins, 
                struct item &itm, struct best_cut &cut, struct context &ctx)
{
        struct item itm_lf;
        struct item itm_rf;

        /* creates first itm with left fragment*/
        ctx.itms_count++;
        itm_lf.id = ctx.itms_count;
        itm_lf.frag_id = frag_id_count;
        itm_lf.is_allocated = YES;
        itm_lf.is_frag = YES;
        itm_lf.is_fragmented = NO;
        itm_lf.nbr_cut = 0;
        itm_lf.size = cut.lf_size;
        itm_lf.tc.v_tasks = cut.c.v_tasks_lf;

        /* creates second itm with right fragment*/
        ctx.itms_count++;
        itm_rf.id = ctx.itms_count;
        itm_rf.frag_id = frag_id_count;
        itm_rf.is_allocated = YES;
        itm_rf.is_frag = YES;
        itm_rf.is_fragmented = NO;
        itm_rf.nbr_cut = 0;
        itm_rf.size = cut.rf_size;
        itm_rf.tc.v_tasks = cut.c.v_tasks_rf;

        printf("\nLeft Fragment %d has been created from Item %d with size %d\n", 
                        itm_lf.id, itm.id, itm_lf.size);
        printf("Right Fragment %d has been created from Item %d with size %d\n\n", 
                        itm_rf.id, itm.id, itm_rf.size);

        if (itm_lf.size == 0 || itm_rf.size == 0) {
                printf("ERR Fragment size = 0!\n");
                exit(0);
        }

        /* store fragments */
        frags_bfdu_f.push_back(itm_lf);
        frags_bfdu_f.push_back(itm_rf);

        /* add fragments to their respective bins */
        add_itm_to_bin(v_bins, itm_lf, cut.target_bin_lf, ctx);
        add_itm_to_bin(v_bins, itm_rf, cut.target_bin_rf, ctx);

        frag_id_count++;
}

void bfdu_f(vector<struct item> &v_itms, vector<struct bin> &v_bins, 
                struct context &ctx)
{
        int ret;
        int alloc_count;

        /* STEP - 1, place all possible items in bins using BFDU */
        printf("\n<--------------------------------------->\n");
        printf("STEP 1, BFDU_F\n");
        printf("<--------------------------------------->\n");
        while(alloc_count != ctx.prm.n) {
                alloc_count = 0;
                for (int i = 0; i < ctx.prm.n; i++) {
                        if (v_itms[i].is_allocated == YES) 
                                continue;

                        /* find best bin to fit itm */
                        ret = _find_best_bin(v_bins, v_itms[i], ctx);

                        /* bin found add itm to it */
                        if (ret > -1) {
                                printf("Best Bin to accomodate Item %d is Bin %d\n", 
                                                v_itms[i].id, ret);
                                add_itm_to_bin(v_bins, v_itms[i], ret, ctx);
                                v_itms[i].is_allocated = YES;

                                /* no bin was found */
                        } else if (ret == -1) {
                                printf("No Bin was found to accomodate Item %d\n", 
                                                v_itms[i].id);

                                ret = _find_best_cut(v_bins, v_itms[i], ctx);
                                if (ret == YES) {
                                        printf("Found Cut %d for Item %d\n", 
                                                        cut.id, v_itms[i].id);
                                        _bff(v_itms, v_bins, v_itms[i], cut, ctx);
                                        v_itms[i].is_allocated = YES;
                                        v_itms[i].is_fragmented = YES;
                                        continue;
                                } 
                                if (ret == NO) {
                                        printf("No Cut was found to accomodate Item %d\n\n", v_itms[i].id);
                                        add_bin(v_bins, ctx);
                                        ctx.cycl_count++;
                                        continue;
                                }

                                /* size bigger than phi */
                        } else if (ret == -2) {
                                printf("Item %d of size %d bigger than PHI\n", 
                                                v_itms[i].id, v_itms[i].size);

                                ret = _find_best_cut(v_bins, v_itms[i], ctx);
                                if (ret == YES) {
                                        printf("Found Cut %d for Item %d\n", 
                                                        cut.id, v_itms[i].id);
                                        _bff(v_itms, v_bins, v_itms[i], cut, ctx);
                                        v_itms[i].is_allocated = YES;
                                        v_itms[i].is_fragmented = YES;
                                        continue;
                                } 
                                if (ret == NO) {
                                        printf("No Cut was found to accomodate Item %d\n\n", v_itms[i].id);
                                        add_bin(v_bins, ctx);
                                        ctx.cycl_count++;
                                        continue;
                                }
                        }
                }
                /* count remaining item to be allocated */
                for (int i = 0; i < ctx.prm.n; i++) {
                        if (v_itms[i].is_allocated == YES)
                                alloc_count++;
                }
        }
}

vector<struct item> *get_frags_bfdu_f(void)
{
        return &frags_bfdu_f;
}
