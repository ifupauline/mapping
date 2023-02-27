#include "optimization.h"
#include "sched_analysis.h"

void priority_optimization(vector<struct bin> &v_bins, struct context &ctx)
{
        int p;

        /* detect items not schedulable */
        for (unsigned int i = 0; i < v_bins.size(); i++) {
                for (unsigned int j = 0; j < v_bins[i].v_itms.size(); j++) {
                        for (unsigned int k = 0; k < v_bins[i].v_itms[j].tc.v_tasks.size(); k++) {
                                if (v_bins[i].v_itms[j].tc.v_tasks[k].r  == -1) {
                                        printf("Core %d, task-chain %d tau %d wcrt failed\n", 
                                                        v_bins[i].id, 
                                                        v_bins[i].v_itms[j].id, 
                                                        v_bins[i].v_itms[j].tc.v_tasks[k].id);

                                        /* save priority of unscheduled task */
                                        p = v_bins[i].v_itms[j].tc.v_tasks[k].p;
                                        priority_reassignment(v_bins[i], j, p);
                                }
                        }
                }
        }
}
