#include "print.h"
#include "mapping.h"
#include "generator.h"

int main(int argc, char **argv)
{
        struct params prm;
        struct context ctx;
        vector<struct bin> v_bins;
        vector<struct item> v_itms;
        struct context ctx_bfdu_f;
        struct context ctx_wfdu_f;
        struct context ctx_frst_f;
        vector<struct bin> v_bins_bfdu_f;
        vector<struct bin> v_bins_wfdu_f;
        vector<struct bin> v_bins_frst_f;
        vector<struct item> v_itms_bfdu_f;
        vector<struct item> v_itms_wfdu_f;
        vector<struct item> v_itms_frst_f;
        vector<struct ecu> v_ecus_bfdu_f;
        vector<struct ecu> v_ecus_wfdu_f;
        vector<struct ecu> v_ecus_frst_f;

        /* parameters */
        input_case_study(argc, argv, prm);

        /* generate set of task-chains and initialize context */
        gen_case_study(v_itms, prm, ctx);
        print_task_chains(v_itms);

        /* offline fragmentation */
        fragmentation(v_itms, ctx);

        /* cmp min bins req */
        init_ctx(v_itms, prm, ctx);

        /* copy instances and select algorithm */
        ctx_bfdu_f = ctx;
        ctx_wfdu_f = ctx;
        ctx_frst_f = ctx;
        ctx_bfdu_f.prm.a = BFDU_F;
        ctx_wfdu_f.prm.a = WFDU_F;
        ctx_frst_f.prm.a = FFDU_F;

        v_itms_bfdu_f = v_itms;
        v_itms_wfdu_f = v_itms;
        v_itms_frst_f = v_itms;
        v_bins_bfdu_f = v_bins;
        v_bins_wfdu_f = v_bins;
        v_bins_frst_f = v_bins;

        /* generation */
        generation(v_bins_bfdu_f, ctx_bfdu_f);
        generation(v_bins_wfdu_f, ctx_wfdu_f);
        generation(v_bins_frst_f, ctx_frst_f);

        /* allocation */
        allocation(v_itms_bfdu_f, v_bins_bfdu_f, ctx_bfdu_f);
        allocation(v_itms_wfdu_f, v_bins_wfdu_f, ctx_wfdu_f);
        allocation(v_itms_frst_f, v_bins_frst_f, ctx_frst_f);

        /* schedulability analysis */
        schedulability_analysis(v_bins_bfdu_f, ctx_bfdu_f);
        schedulability_analysis(v_bins_wfdu_f, ctx_wfdu_f);
        schedulability_analysis(v_bins_frst_f, ctx_frst_f);

        /* optimization */
        optimization(v_bins_bfdu_f, ctx_bfdu_f);
        optimization(v_bins_wfdu_f, ctx_wfdu_f);
        optimization(v_bins_frst_f, ctx_frst_f);

        /* communication */
        communication(v_bins_bfdu_f, v_itms_bfdu_f, v_ecus_bfdu_f, ctx_bfdu_f);
        communication(v_bins_wfdu_f, v_itms_wfdu_f, v_ecus_wfdu_f, ctx_wfdu_f);
        communication(v_bins_frst_f, v_itms_frst_f, v_ecus_frst_f, ctx_frst_f);

        /* results */
        print_cores(v_bins_bfdu_f, ctx_bfdu_f);
        print_cores(v_bins_wfdu_f, ctx_wfdu_f);
        print_cores(v_bins_frst_f, ctx_frst_f);
        print_vectors(v_bins_bfdu_f, v_itms_bfdu_f, ctx_bfdu_f);
        print_vectors(v_bins_wfdu_f, v_itms_wfdu_f, ctx_wfdu_f);
        print_vectors(v_bins_frst_f, v_itms_frst_f, ctx_frst_f);
        print_stats(v_itms_bfdu_f, v_bins_bfdu_f, ctx_bfdu_f);
        print_stats(v_itms_wfdu_f, v_bins_wfdu_f, ctx_wfdu_f);
        print_stats(v_itms_frst_f, v_bins_frst_f, ctx_frst_f);

        return 0;
}
