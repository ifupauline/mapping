#include "../include/print.h"

int main(void)
{
        struct params prm;
        struct context ctx;
        vector<struct item> lst_itms;
        vector<struct bin> lst_bins;

        struct context ctx_bfdu_f;
        struct context ctx_wfdu_f;
        vector<struct item> lst_itms_bfdu_f;
        vector<struct item> lst_itms_wfdu_f;
        vector<struct bin> lst_bins_bfdu_f;
        vector<struct bin> lst_bins_wfdu_f;

        /* parameters */
        prm.n = 100;
        prm.s = 100;
        prm.c = 100;
        prm.cp = 4;
        prm.a = -1;
        prm.phi = 80;

        /* generate set of items and initialize context */
        gen_data_set(lst_itms, prm);

        /* cmp min bins req */
        init_ctx(prm, ctx);
        comp_min_bins(lst_itms, ctx);

        /* copy instances and select algo */
        ctx_bfdu_f = ctx;
        ctx_wfdu_f = ctx;
        ctx_bfdu_f.prm.a = BFDU_F;
        ctx_wfdu_f.prm.a = WFDU_F;

        lst_itms_bfdu_f = lst_itms;
        lst_itms_wfdu_f = lst_itms;
        lst_bins_bfdu_f = lst_bins;
        lst_bins_wfdu_f = lst_bins;

        /* generation */
        generation(lst_bins_bfdu_f, ctx_bfdu_f);
        generation(lst_bins_wfdu_f, ctx_wfdu_f);

        /* reduction */
        reduction(lst_itms_bfdu_f, lst_bins_bfdu_f, ctx_bfdu_f);
        reduction(lst_itms_wfdu_f, lst_bins_wfdu_f, ctx_wfdu_f);

        /* allocation */
        allocation(lst_itms_bfdu_f, lst_bins_bfdu_f, ctx_bfdu_f);
        allocation(lst_itms_wfdu_f, lst_bins_wfdu_f, ctx_wfdu_f);

        /* results */
        print_lst_itms(lst_itms, ctx);
        print_lst_bins(lst_bins_bfdu_f, ctx_bfdu_f);
        print_lst_bins(lst_bins_wfdu_f, ctx_wfdu_f);
        print_vectors(lst_itms_bfdu_f, ctx_bfdu_f);
        print_vectors(lst_itms_wfdu_f, ctx_wfdu_f);
        print_stats(lst_itms_bfdu_f, lst_bins_bfdu_f, ctx_bfdu_f);
        print_stats(lst_itms_wfdu_f, lst_bins_wfdu_f, ctx_wfdu_f);

        return 0;
}
