#include "print.h"
#include "generator.h"
#include "optimization.h"
#include "sched_analysis.h"

static void _input(int argc, char **argv, struct params &prm)
{
        if (argc != 4) {
                printf("Wrong Number of Arguments!\n");
                exit(0);
        }
        prm.n = atoi(argv[1]);
        prm.phi = atoi(argv[2]);
        prm.h = atoi(argv[3]);
        check_params(prm);
}

int main(int argc, char **argv)
{
        struct params prm;
        struct context ctx;
        vector<struct item> v_itms;
        vector<struct bin> v_bins;

        struct context ctx_bfdu_f;
        vector<struct item> v_itms_bfdu_f;
        vector<struct bin> v_bins_bfdu_f;

        /* TODO 
         * - integrate priority optimization in worst-case analysis  
         * - design swapping algorithm  
         * - improve reduction
         * - move input function in generator.cpp
         * - fix bench1.cpp
         * - reassign priority for displacement optimization
         * - massive refact for optimization, build reusable primitives
         */

        /* parameters */
        _input(argc, argv, prm);

        /* generate set of task-chains and initialize context */
        gen_tc_set(v_itms, prm, ctx);

        /* cmp min bins req */
        init_ctx(v_itms, prm, ctx);

        /* copy instances and select algorithm */
        ctx_bfdu_f = ctx;
        ctx_bfdu_f.prm.a = BFDU_F;

        v_itms_bfdu_f = v_itms;
        v_bins_bfdu_f = v_bins;

        /* generation */
        generation(v_bins_bfdu_f, ctx_bfdu_f);
        print_task_chains(v_itms);

        /* reduction */
        reduction(v_itms_bfdu_f, v_bins_bfdu_f, ctx_bfdu_f);

        /* allocation */
        allocation(v_itms_bfdu_f, v_bins_bfdu_f, ctx_bfdu_f);

        /* worst-case analysis */
        worst_case_analysis(v_bins_bfdu_f, ctx_bfdu_f);

        /* priority assignment optimization */
        optimization(v_bins_bfdu_f, ctx_bfdu_f);

        /* results */
        print_cores(v_bins_bfdu_f, ctx_bfdu_f);
        print_vectors(v_bins_bfdu_f, v_itms_bfdu_f, ctx_bfdu_f);
        print_stats(v_itms_bfdu_f, v_bins_bfdu_f, ctx_bfdu_f);

        displacement_optimization(v_bins_bfdu_f, ctx_bfdu_f);

        return 0;
}
