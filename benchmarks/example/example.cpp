#include "print.h"
#include "generator.h"
#include "sched_analysis.h"

int main(void)
{
        struct params prm;
        struct context ctx;
        vector<struct item> v_itms;
        vector<struct bin> v_bins;

        struct context ctx_bfdu_f;
        struct context ctx_wfdu_f;
        vector<struct item> v_itms_bfdu_f;
        vector<struct item> v_itms_wfdu_f;
        vector<struct bin> v_bins_bfdu_f;
        vector<struct bin> v_bins_wfdu_f;

        /* parameters */
        prm.n = 10;
        prm.c = 100;
        prm.phi = 80;
        prm.s = prm.c * 0.20;  /* max utilization rate for a task is 20% of C */
        prm.fr = prm.n * 0.10; /* fragmentation rate, that is 50% of tc > phi */

        /* generate set of task-chains and initialize context */
        gen_tc_set(v_itms, prm);
        print_task_chains(v_itms);

        /* cmp min bins req */
        init_ctx(prm, ctx);
        comp_min_bins(v_itms, ctx);

        /* copy instances and select algo */
        ctx_bfdu_f = ctx;
        ctx_wfdu_f = ctx;
        ctx_bfdu_f.prm.a = BFDU_F;
        ctx_wfdu_f.prm.a = WFDU_F;

        v_itms_bfdu_f = v_itms;
        v_itms_wfdu_f = v_itms;
        v_bins_bfdu_f = v_bins;
        v_bins_wfdu_f = v_bins;

        /* generation */
        generation(v_bins_bfdu_f, ctx_bfdu_f);
        generation(v_bins_wfdu_f, ctx_wfdu_f);

        /* reduction */
        reduction(v_itms_bfdu_f, v_bins_bfdu_f, ctx_bfdu_f);
        reduction(v_itms_wfdu_f, v_bins_wfdu_f, ctx_wfdu_f);

        /* allocation */
        allocation(v_itms_bfdu_f, v_bins_bfdu_f, ctx_bfdu_f);
        allocation(v_itms_wfdu_f, v_bins_wfdu_f, ctx_wfdu_f);

        /* results */
        print_v_itms(v_itms, ctx);
        print_v_bins(v_bins_bfdu_f, ctx_bfdu_f);
        print_v_bins(v_bins_wfdu_f, ctx_wfdu_f);
        print_vectors(v_itms_bfdu_f, ctx_bfdu_f);
        print_vectors(v_itms_wfdu_f, ctx_wfdu_f);
        print_stats(v_itms_bfdu_f, v_bins_bfdu_f, ctx_bfdu_f);
        print_stats(v_itms_wfdu_f, v_bins_wfdu_f, ctx_wfdu_f);

        

        return 0;
}
