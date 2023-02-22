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
        vector<struct item> v_itms_bfdu_f;
        vector<struct bin> v_bins_bfdu_f;

        /* TODO 
         * - generate harmonic task-chains
         * - fix k_500 tests
         * - create number of cuts  
         */

        /* parameters */
        prm.n = 100;
        prm.c = 100;
        prm.phi = 80;
        prm.max_tu = 20;  /* max utilization rate for a task in percent */
        prm.fr = 5;      /* fragmentation rate in number of items > phi */

        /* generate set of task-chains and initialize context */
        gen_tc_set(v_itms, prm);

        /* cmp min bins req */
        init_ctx(prm, ctx);
        cmp_min_bins(v_itms, ctx);

        /* copy instances and select algorithm */
        ctx_bfdu_f = ctx;
        ctx_bfdu_f.prm.a = BFDU_F;

        v_itms_bfdu_f = v_itms;
        v_bins_bfdu_f = v_bins;

        /* generation */
        generation(v_bins_bfdu_f, ctx_bfdu_f);

        /* reduction */
        reduction(v_itms_bfdu_f, v_bins_bfdu_f, ctx_bfdu_f);

        /* allocation */
        allocation(v_itms_bfdu_f, v_bins_bfdu_f, ctx_bfdu_f);

        /* worst-case analysis */
        worst_case_analysis(v_bins_bfdu_f, ctx_bfdu_f);

        /* results */
        print_task_chains(v_itms);
        print_bins(v_bins_bfdu_f, ctx_bfdu_f);
        print_vectors(v_bins_bfdu_f, v_itms_bfdu_f, ctx_bfdu_f);
        print_stats(v_itms_bfdu_f, v_bins_bfdu_f, ctx_bfdu_f);

        return 0;
}
