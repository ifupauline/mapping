#include "print.h"
#include "mapping.h"
#include "generator.h"

int main(int argc, char **argv)
{
redo:   struct params prm;
        struct context ctx;
        vector<struct core> v_cores;
        vector<struct tc> v_tcs;
        struct context ctx_bfdu;
        struct context ctx_wfdu;
        struct context ctx_ffdu;
        vector<struct core> v_cores_bfdu;
        vector<struct core> v_cores_wfdu;
        vector<struct core> v_cores_ffdu;
        vector<struct tc> v_tcs_bfdu;
        vector<struct tc> v_tcs_wfdu;
        vector<struct tc> v_tcs_ffdu;

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
        ctx_bfdu = ctx;
        ctx_wfdu = ctx;
        ctx_ffdu = ctx;
        ctx_bfdu.prm.a = BFDU;
        ctx_wfdu.prm.a = WFDU;
        ctx_ffdu.prm.a = FFDU;

        v_tcs_bfdu = v_tcs;
        v_tcs_wfdu = v_tcs;
        v_tcs_ffdu = v_tcs;
        v_cores_bfdu = v_cores;
        v_cores_wfdu = v_cores;
        v_cores_ffdu = v_cores;

        /* create cores */
        gen_arch(v_cores_bfdu, ctx_bfdu);
        gen_arch(v_cores_wfdu, ctx_wfdu);
        gen_arch(v_cores_ffdu, ctx_ffdu);

        /* allocation */
        allocation(v_tcs_bfdu, v_cores_bfdu, ctx_bfdu);
        if (STATE == FAILED) {
                STATE = OK;
                goto redo;
        }
        allocation(v_tcs_wfdu, v_cores_wfdu, ctx_wfdu);
        if (STATE == FAILED) {
                STATE = OK;
                goto redo;
        }
        allocation(v_tcs_ffdu, v_cores_ffdu, ctx_ffdu);
        if (STATE == FAILED) {
                STATE = OK;
                goto redo;
        }

        /* schedulability analysis */
        schedulability_analysis(v_cores_bfdu, ctx_bfdu);
        schedulability_analysis(v_cores_wfdu, ctx_wfdu);
        schedulability_analysis(v_cores_ffdu, ctx_ffdu);

        /* optimization */
        placement(v_cores_bfdu, ctx_bfdu);
        placement(v_cores_wfdu, ctx_wfdu);
        placement(v_cores_ffdu, ctx_ffdu);

        /* results */
        print_cores(v_cores_bfdu, ctx_bfdu);
        print_cores(v_cores_wfdu, ctx_wfdu);
        print_cores(v_cores_ffdu, ctx_ffdu);
        print_vectors(v_cores_bfdu, v_tcs_bfdu, ctx_bfdu);
        print_vectors(v_cores_wfdu, v_tcs_wfdu, ctx_wfdu);
        print_vectors(v_cores_ffdu, v_tcs_ffdu, ctx_ffdu);
        print_stats(v_tcs_bfdu, v_cores_bfdu, ctx_bfdu);
        print_stats(v_tcs_wfdu, v_cores_wfdu, ctx_wfdu);
        print_stats(v_tcs_ffdu, v_cores_ffdu, ctx_ffdu);

        return 0;
}
