#include "print.h"
#include "mapping.h"
#include "generator.h"

int main(int argc, char **argv)
{
        struct params prm;
        struct context ctx;
        vector<struct item> v_itms;
        vector<struct bin> v_bins;

        struct context ctx_bfdu_f;
        vector<struct item> v_itms_bfdu_f;
        vector<struct bin> v_bins_bfdu_f;

        /* parameters */
        input(argc, argv, prm);

        /* generate set of task-chains and initialize context */
        gen_tc_set(v_itms, prm, ctx);
        print_task_chains(v_itms);

        /* offline fragmentation */
        fragmentation(v_itms, ctx);

        /* cmp min bins req */
        init_ctx(v_itms, prm, ctx);

        /* copy instances and select algorithm */
        ctx_bfdu_f = ctx;
        ctx_bfdu_f.prm.a = BFDU_F;

        v_itms_bfdu_f = v_itms;
        v_bins_bfdu_f = v_bins;

        /* generation */
        generation(v_bins_bfdu_f, ctx_bfdu_f);

        /* allocation */
        allocation(v_itms_bfdu_f, v_bins_bfdu_f, ctx_bfdu_f);

        /* schedulability analysis */
        schedulability_analysis(v_bins_bfdu_f, ctx_bfdu_f);

        /* optimization */
        optimization(v_bins_bfdu_f, ctx_bfdu_f);

        /* results */
        print_cores(v_bins_bfdu_f, ctx_bfdu_f);
        print_vectors(v_bins_bfdu_f, v_itms_bfdu_f, ctx_bfdu_f);
        print_stats(v_itms_bfdu_f, v_bins_bfdu_f, ctx_bfdu_f);

        return 0;
}
