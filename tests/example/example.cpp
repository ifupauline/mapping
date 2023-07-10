#include "print.h"
#include "mapping.h"
#include "generator.h"

int main(int argc, char **argv)
{
        struct params prm;
        struct context ctx;
        struct context ctx_bfdu_500;
        struct context ctx_bfdu_800;
        struct context ctx_wfdu_500;
        struct context ctx_wfdu_800;
        struct context ctx_frst_500;
        struct context ctx_frst_800;

        vector<struct bin> v_bins;
        vector<struct item> v_itms;
        
        vector<struct bin> v_bins_bfdu_500;
        vector<struct bin> v_bins_bfdu_800;
        vector<struct bin> v_bins_wfdu_500;
        vector<struct bin> v_bins_wfdu_800;
        vector<struct bin> v_bins_frst_500;
        vector<struct bin> v_bins_frst_800;
        vector<struct item> v_itms_bfdu_500;
        vector<struct item> v_itms_bfdu_800;
        vector<struct item> v_itms_wfdu_500;
        vector<struct item> v_itms_wfdu_800;
        vector<struct item> v_itms_frst_500;
        vector<struct item> v_itms_frst_800;
        vector<struct ecu> v_ecus_bfdu_500;
        vector<struct ecu> v_ecus_bfdu_800;
        vector<struct ecu> v_ecus_wfdu_500;
        vector<struct ecu> v_ecus_wfdu_800;
        vector<struct ecu> v_ecus_frst_500;
        vector<struct ecu> v_ecus_frst_800;

        /* parameters */
        prm = {0};

        /* generate set of task-chains and initialize context */
        gen_case_study(v_itms, prm, ctx);
        print_task_chains(v_itms);

        v_itms_bfdu_500 = v_itms;
        v_itms_bfdu_800 = v_itms;
        v_itms_wfdu_500 = v_itms;
        v_itms_wfdu_800 = v_itms;
        v_itms_frst_500 = v_itms;
        v_itms_frst_800 = v_itms;

        ctx.prm = prm;
        ctx_bfdu_500 = ctx;
        ctx_bfdu_800 = ctx;
        ctx_wfdu_500 = ctx;
        ctx_wfdu_800 = ctx;
        ctx_frst_500 = ctx;
        ctx_frst_800 = ctx;

        ctx_bfdu_500.prm.e = 500;
        ctx_bfdu_800.prm.e = 800;
        ctx_wfdu_500.prm.e = 500;
        ctx_wfdu_800.prm.e = 800;
        ctx_frst_500.prm.e = 500;
        ctx_frst_800.prm.e = 800;

        /* offline fragmentation */
        fragmentation(v_itms_bfdu_500, ctx_bfdu_500);
        fragmentation(v_itms_bfdu_800, ctx_bfdu_800);
        fragmentation(v_itms_wfdu_500, ctx_wfdu_500);
        fragmentation(v_itms_wfdu_800, ctx_wfdu_800);
        fragmentation(v_itms_frst_500, ctx_frst_500);
        fragmentation(v_itms_frst_800, ctx_frst_800);

        /* cmp min bins req */
        init_ctx(v_itms_bfdu_500, ctx_bfdu_500.prm, ctx_bfdu_500);
        init_ctx(v_itms_bfdu_800, ctx_bfdu_800.prm, ctx_bfdu_800);
        init_ctx(v_itms_wfdu_500, ctx_wfdu_500.prm, ctx_wfdu_500);
        init_ctx(v_itms_wfdu_800, ctx_wfdu_800.prm, ctx_wfdu_800);
        init_ctx(v_itms_frst_500, ctx_frst_500.prm, ctx_frst_500);
        init_ctx(v_itms_frst_800, ctx_frst_800.prm, ctx_frst_800);

        /* copy instances and select algorithm */
        ctx_bfdu_500.prm.a = BFDU_F;
        ctx_bfdu_800.prm.a = BFDU_F;
        ctx_wfdu_500.prm.a = WFDU_F;
        ctx_wfdu_800.prm.a = WFDU_F;
        ctx_frst_500.prm.a = FFDU_F;
        ctx_frst_800.prm.a = FFDU_F;

        /* generation */
        generation(v_bins_bfdu_500, ctx_bfdu_500);
        generation(v_bins_bfdu_800, ctx_bfdu_800);
        generation(v_bins_wfdu_500, ctx_wfdu_500);
        generation(v_bins_wfdu_800, ctx_wfdu_800);
        generation(v_bins_frst_500, ctx_frst_500);
        generation(v_bins_frst_800, ctx_frst_800);

        /* allocation */
        allocation(v_itms_bfdu_500, v_bins_bfdu_500, ctx_bfdu_500);
        allocation(v_itms_bfdu_800, v_bins_bfdu_800, ctx_bfdu_800);
        allocation(v_itms_wfdu_500, v_bins_wfdu_500, ctx_wfdu_500);
        allocation(v_itms_wfdu_800, v_bins_wfdu_800, ctx_wfdu_800);
        allocation(v_itms_frst_500, v_bins_frst_500, ctx_frst_500);
        allocation(v_itms_frst_800, v_bins_frst_800, ctx_frst_800);

        /* schedulability analysis */
        schedulability_analysis(v_bins_bfdu_500, ctx_bfdu_500);
        schedulability_analysis(v_bins_bfdu_800, ctx_bfdu_800);
        schedulability_analysis(v_bins_wfdu_500, ctx_wfdu_500);
        schedulability_analysis(v_bins_wfdu_800, ctx_wfdu_800);
        schedulability_analysis(v_bins_frst_500, ctx_frst_500);
        schedulability_analysis(v_bins_frst_800, ctx_frst_800);

        /* optimization */
        optimization(v_bins_bfdu_500, ctx_bfdu_500);
        optimization(v_bins_bfdu_800, ctx_bfdu_800);
        optimization(v_bins_wfdu_500, ctx_wfdu_500);
        optimization(v_bins_wfdu_800, ctx_wfdu_800);
        optimization(v_bins_frst_500, ctx_frst_500);
        optimization(v_bins_frst_800, ctx_frst_800);

        /* communication */
        communication(v_bins_bfdu_500, v_itms_bfdu_500, v_ecus_bfdu_500, ctx_bfdu_500);
        communication(v_bins_bfdu_800, v_itms_bfdu_800, v_ecus_bfdu_800, ctx_bfdu_800);
        communication(v_bins_wfdu_500, v_itms_wfdu_500, v_ecus_wfdu_500, ctx_wfdu_500);
        communication(v_bins_wfdu_800, v_itms_wfdu_800, v_ecus_wfdu_800, ctx_wfdu_800);
        communication(v_bins_frst_500, v_itms_frst_500, v_ecus_frst_500, ctx_frst_500);
        communication(v_bins_frst_800, v_itms_frst_800, v_ecus_frst_800, ctx_frst_800);

        /* results */
        //print_cores(v_bins_bfdu_500, ctx_bfdu_500);
        //print_cores(v_bins_bfdu_800, ctx_bfdu_800);
        //print_cores(v_bins_wfdu_500, ctx_wfdu_500);
        //print_cores(v_bins_wfdu_800, ctx_wfdu_800);
        //print_cores(v_bins_frst_500, ctx_frst_500);
        //print_cores(v_bins_frst_800, ctx_frst_800);
        print_vectors(v_bins_bfdu_500, v_itms_bfdu_500, ctx_bfdu_500);
        print_vectors(v_bins_bfdu_800, v_itms_bfdu_800, ctx_bfdu_800);
        print_vectors(v_bins_wfdu_500, v_itms_wfdu_500, ctx_wfdu_500);
        print_vectors(v_bins_wfdu_800, v_itms_wfdu_800, ctx_wfdu_800);
        print_vectors(v_bins_frst_500, v_itms_frst_500, ctx_frst_500);
        print_vectors(v_bins_frst_800, v_itms_frst_800, ctx_frst_800);
        print_stats(v_itms_bfdu_500, v_bins_bfdu_500, ctx_bfdu_500);
        print_stats(v_itms_bfdu_800, v_bins_bfdu_800, ctx_bfdu_800);
        print_stats(v_itms_wfdu_500, v_bins_wfdu_500, ctx_wfdu_500);
        print_stats(v_itms_wfdu_800, v_bins_wfdu_800, ctx_wfdu_800);
        print_stats(v_itms_frst_500, v_bins_frst_500, ctx_frst_500);
        print_stats(v_itms_frst_800, v_bins_frst_800, ctx_frst_800);

        return 0;
}
