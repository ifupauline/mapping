#include "print.h"
#include "mapping.h"
#include "generator.h"

int main(int argc, char **argv)
{
        struct params prm;
        struct context ctx;
        vector<struct core> v_cores;
        vector<struct tc> v_tcs;
        struct context ctx_bfdu_f;
        struct context ctx_wfdu_f;
        struct context ctx_ffdu_f;
        vector<struct core> v_cores_bfdu_f;
        vector<struct core> v_cores_wfdu_f;
        vector<struct core> v_cores_ffdu_f;
        vector<struct tc> v_tcs_bfdu_f;
        vector<struct tc> v_tcs_wfdu_f;
        vector<struct tc> v_tcs_ffdu_f;

        /* parameters */
        input_prm(argc, argv, prm);

        /* generate set of task-chains and initialize context */
        gen_app(v_tcs, prm, ctx);
        print_task_chains(v_tcs);

        /* offline partitioning */
        partitioning(v_tcs, ctx);

        /* cmp min cores req */
        init_ctx(v_tcs, prm, ctx);

        /* copy instances and select algorithm */
        ctx_bfdu_f = ctx;
        ctx_wfdu_f = ctx;
        ctx_ffdu_f = ctx;
        ctx_bfdu_f.prm.a = BFDU_F;
        ctx_wfdu_f.prm.a = WFDU_F;
        ctx_ffdu_f.prm.a = FFDU_F;

        v_tcs_bfdu_f = v_tcs;
        v_tcs_wfdu_f = v_tcs;
        v_tcs_ffdu_f = v_tcs;
        v_cores_bfdu_f = v_cores;
        v_cores_wfdu_f = v_cores;
        v_cores_ffdu_f = v_cores;

        /* create cores */
        gen_arch(v_cores_bfdu_f, ctx_bfdu_f);
        gen_arch(v_cores_wfdu_f, ctx_wfdu_f);
        gen_arch(v_cores_ffdu_f, ctx_ffdu_f);

        /* allocation */
        allocation(v_tcs_bfdu_f, v_cores_bfdu_f, ctx_bfdu_f);
        allocation(v_tcs_wfdu_f, v_cores_wfdu_f, ctx_wfdu_f);
        allocation(v_tcs_ffdu_f, v_cores_ffdu_f, ctx_ffdu_f);

        /* schedulability analysis */
        schedulability_analysis(v_cores_bfdu_f, ctx_bfdu_f);
        schedulability_analysis(v_cores_wfdu_f, ctx_wfdu_f);
        schedulability_analysis(v_cores_ffdu_f, ctx_ffdu_f);

        /* optimization */
        placement(v_cores_bfdu_f, ctx_bfdu_f);
        placement(v_cores_wfdu_f, ctx_wfdu_f);
        placement(v_cores_ffdu_f, ctx_ffdu_f);

        /* results */
        print_cores(v_cores_bfdu_f, ctx_bfdu_f);
        print_cores(v_cores_wfdu_f, ctx_wfdu_f);
        print_cores(v_cores_ffdu_f, ctx_ffdu_f);
        print_vectors(v_cores_bfdu_f, v_tcs_bfdu_f, ctx_bfdu_f);
        print_vectors(v_cores_wfdu_f, v_tcs_wfdu_f, ctx_wfdu_f);
        print_vectors(v_cores_ffdu_f, v_tcs_ffdu_f, ctx_ffdu_f);
        print_stats(v_tcs_bfdu_f, v_cores_bfdu_f, ctx_bfdu_f);
        print_stats(v_tcs_wfdu_f, v_cores_wfdu_f, ctx_wfdu_f);
        print_stats(v_tcs_ffdu_f, v_cores_ffdu_f, ctx_ffdu_f);

        return 0;
}
