#include "let.h"
#include "print.h" /* tmp */
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
                struct context &ctx, int &best_load, int &best_gcd)
{
        int best_rem = ctx.prm.phi;
        int bin_id;
        int tmp_rem;
        int tmp_load;
        int tmp_gcd;
        int is_found;

        bin_id = 0;
        tmp_load = 0;
        tmp_rem = 0;
        tmp_gcd = 0;
        is_found = NO;
        best_rem = ctx.prm.phi;

        if (itm.size > ctx.prm.phi)
                return -2;

        for (unsigned int i = 0; i < v_bins.size(); i++) {
                tmp_load = check_if_fit_itm(v_bins[i], itm, tmp_gcd);
                if (tmp_load <= v_bins[i].phi) {
                        tmp_rem = v_bins[i].load_rem - tmp_load;

                        if (tmp_rem < best_rem) {
                                best_rem = tmp_rem;
                                best_load = tmp_load;
                                best_gcd = tmp_gcd;
                                bin_id = v_bins[i].id;
                        }
                        is_found = YES;
                        continue;
                }
        }
        if (is_found == YES) 
                return bin_id;

        return -1;
}

static int _find_best_cut(vector<struct bin> &v_bins, struct item &itm,
                struct context &ctx, int &best_l_load, int &best_r_load,
                int &best_l_gcd, int &best_r_gcd)
{
        int l_tmp_rem;
        int r_tmp_rem;

        int l_best_rem;
        int r_best_rem;

        int is_l_val_found;
        int is_r_val_found;

        int is_best_found;
        struct best_cut tmp_cut;

        int tmp_load;
        int tmp_gcd;
        int tmp_min;

        tmp_gcd = 0;
        tmp_load = 0;
        tmp_min = ctx.prm.phi;

        cut = {0};
        is_best_found = NO;

        /* fragment cannot be fragmented */
        if (itm.is_frag == YES) 
                return -1;

        /* check for a cut */
        for (unsigned int i = 0; i < itm.v_cuts.size(); i++) {
                printf("\n\nITERATION %d------------------------>\n", i);
                is_l_val_found = NO;
                is_r_val_found = NO;

                l_tmp_rem = 0;
                r_tmp_rem = 0;

                l_best_rem = ctx.prm.phi;
                r_best_rem = ctx.prm.phi;

                tmp_cut = {0};

                /* left value */
                for (unsigned int j = 0; j < v_bins.size(); j++) {
                        /* check if fragment can be placed in a bin */
                        tmp_load = check_if_fit_cut(v_bins[j], itm.v_cuts[i], tmp_gcd, LEFT);
                        if (tmp_load <= v_bins[i].phi) {
                                is_l_val_found = YES;

                                /* compute the remaining */
                                l_tmp_rem = v_bins[i].phi - tmp_load;

                                /* save the bin for best fit */
                                if (l_tmp_rem < l_best_rem) {
                                        l_best_rem = l_tmp_rem;
                                        best_l_load = tmp_load;
                                        best_l_gcd = tmp_gcd;
                                        tmp_cut.id = itm.v_cuts[i].id;
                                        tmp_cut.c.v_tasks_lf = itm.v_cuts[i].v_tasks_lf;
                                        tmp_cut.target_bin_lf = v_bins[j].id;
                                        tmp_cut.lf_size = itm.v_cuts[i].c_pair.first;
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
                        tmp_load = check_if_fit_cut(v_bins[j], itm.v_cuts[i], tmp_gcd, RIGHT);
                        if (tmp_load <= v_bins[i].phi) {
                                is_r_val_found = YES;

                                /* compute the remaining */
                                r_tmp_rem = v_bins[j].phi - tmp_load;

                                /* save the bin for best fit */
                                if (r_tmp_rem < r_best_rem) {
                                        r_best_rem = r_tmp_rem;
                                        best_r_load = tmp_load;
                                        best_r_gcd = tmp_gcd;
                                        tmp_cut.id = itm.v_cuts[i].id;
                                        tmp_cut.c.v_tasks_rf = itm.v_cuts[i].v_tasks_rf;
                                        tmp_cut.target_bin_rf = v_bins[j].id;
                                        tmp_cut.rf_size = itm.v_cuts[i].c_pair.second;
                                }
                        }
                }

                /* if cut found find the one with the best diff */
                if (is_l_val_found == YES && is_r_val_found == YES) {
                        if (tmp_cut.target_bin_lf == tmp_cut.target_bin_rf) {
                                printf("ERR! Search Cut have found same Bins\n");
                                exit(0);
                        }
                        tmp_cut.diff = l_best_rem + r_best_rem;

                        if (tmp_cut.diff < tmp_min) {
                                tmp_min = tmp_cut.diff;
                                cut = tmp_cut;
                                is_best_found = YES;
                                printf("l_best_rem: %d\n", l_best_rem);
                                printf("r_best_rem: %d\n", r_best_rem);
                                printf("tmp_min: %d\n", tmp_min);
                        }
                }
        }

        if (is_best_found == YES) {
                printf("Best Min Rem %d\n", cut.diff);
                printf("Item %d Best Left Cut %d Bin %d\n", itm.id, cut.id, cut.target_bin_lf);
                printf("Item %d Best Right Cut %d Bin %d\n\n\n\n", itm.id, cut.id, cut.target_bin_rf);
                exit(0);
                return YES;

        } else
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
        itm_lf.disp_count = 0;
        itm_lf.frag_id = frag_id_count;
        itm_lf.is_allocated = YES;
        itm_lf.is_frag = YES;
        itm_lf.is_fragmented = NO;
        itm_lf.nbr_cut = 0;
        itm_lf.size = cut.lf_size;
        itm_lf.v_tasks = cut.c.v_tasks_lf;

        /* creates second itm with right fragment*/
        ctx.itms_count++;
        itm_rf.id = ctx.itms_count;
        itm_rf.disp_count = 0;
        itm_rf.frag_id = frag_id_count;
        itm_rf.is_allocated = YES;
        itm_rf.is_frag = YES;
        itm_rf.is_fragmented = NO;
        itm_rf.nbr_cut = 0;
        itm_rf.size = cut.rf_size;
        itm_rf.v_tasks = cut.c.v_tasks_rf;

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
        //add_itm_to_bin(v_bins, itm_lf, cut.target_bin_lf, ctx);
        //add_itm_to_bin(v_bins, itm_rf, cut.target_bin_rf, ctx);

        frag_id_count++;
}

void bfdu_f(vector<struct item> &v_itms, vector<struct bin> &v_bins, 
                struct context &ctx)
{
        int ret;
        int load;
        int gcd;
        int bin_id;
        int alloc_count;

        int l_load;
        int r_load;
        int l_gcd;
        int r_gcd;

        ret = 0;
        bin_id = 0;

        /* STEP - 1, place all possible items in bins using BFDU */
        printf("\n<--------------------------------------->\n");
        printf("STEP 1, BFDU_F\n");
        printf("<--------------------------------------->\n");
        while(alloc_count != ctx.prm.n) {
                alloc_count = 0;
                for (int i = 0; i < ctx.prm.n; i++) {
                        gcd = 0;
                        load = 0;
                        l_load = 0;
                        r_load = 0;
                        l_gcd = 0;
                        r_gcd = 0;
                        if (v_itms[i].is_allocated == YES) 
                                continue;

                        /* find best bin to fit itm */
                        ret = _find_best_bin(v_bins, v_itms[i], ctx, load, gcd);

                        /* bin found add itm to it */
                        if (ret > -1) {
                                printf("Best Bin to accomodate Item %d is Bin %d with GCD %d\n", 
                                                v_itms[i].id, ret, gcd);
                                bin_id = ret;
                                add_itm_to_bin(v_bins, v_itms[i], bin_id, ctx, load, gcd);
                                v_itms[i].is_allocated = YES;
                                continue;

                                /* no bin was found */
                        } else if (ret == -1) {
                                printf("No Bin was found to accomodate Item %d\n", 
                                                v_itms[i].id);

                                ret = _find_best_cut(v_bins, v_itms[i], ctx, l_load, r_load, l_gcd, r_gcd);
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
                                        exit(0);
                                        add_bin(v_bins, ctx);
                                        ctx.cycl_count++;
                                        continue;
                                }

                                /* size bigger than phi */
                        } else if (ret == -2) {
                                printf("Item %d of size %d bigger than PHI\n", 
                                                v_itms[i].id, v_itms[i].size);

                                ret = _find_best_cut(v_bins, v_itms[i], ctx, l_load, r_load, l_gcd, r_gcd);
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
                                        exit(0);
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
        print_cores(v_bins, ctx);
        exit(0);
}

vector<struct item> *get_frags_bfdu_f(void)
{
        return &frags_bfdu_f;
}
