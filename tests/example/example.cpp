#include "print.h"
#include "mapping.h"
#include "generator.h"

int main(int argc, char **argv)
{
        struct params prm;
        struct context ctx;
        vector<struct item> v_itms;
        vector<struct bin> v_bins;

        /* parameters */
        input(argc, argv, prm);

        /* generate set of task-chains and initialize context */
        gen_tc_set(v_itms, prm, ctx);
        print_task_chains(v_itms);

        /* offline fragmentation */
        fragmentation(v_itms, ctx);

        /* cmp min bins req */
        init_ctx(v_itms, prm, ctx);

        /* generation */
        generation(v_bins, ctx);

        /* allocation */
        allocation(v_itms, v_bins, ctx);

        /* schedulability analysis */
        schedulability_analysis(v_bins, ctx);

        /* optimization */
        optimization(v_bins, ctx);

        /* results */
        print_cores(v_bins, ctx);
        print_vectors(v_bins, v_itms, ctx);
        print_stats(v_itms, v_bins, ctx);

        return 0;
}
