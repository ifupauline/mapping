#include "print.h"
#include "generator.h"
#include "sched_analysis.h"

int main(void)
{
        vector<struct task_chain> v_tc;
        struct params prm;

        prm.n = 100;
        prm.c = 100;
        prm.phi = 80;
        prm.s = prm.c * 0.20; /* max utilization rate for a task is 20% of C */
        prm.fr = prm.n * 0.20; /* fragmentation rate, that is 50% of tc > phi */
        prm.cp = 2;

        gen_tc_set(v_tc, prm);

        print_task_chains(v_tc);

        return 0;
}
