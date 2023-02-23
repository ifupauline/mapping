#include "optimization.h"
#include "sched_analysis.h"
#include "print.h"

static void search_bin(vector<struct bin> &v_bc, struct item &itm, struct context &ctx)
{
        int flag = NO;

        for (unsigned int i = 0; i < v_bc.size(); i++) {
                if (v_bc[i].cap_rem >= itm.size) {
                        printf("Found Bin %d with cap_rem %d to accomodate Item %d\n\n", v_bc[i].id, v_bc[i].cap_rem, itm.id);
                        flag = YES;
                        v_bc[i].v_itms.push_back(itm);
                        v_bc[i].cap_rem -= itm.size;
                        break;
                }
        }
        if (flag == NO)
                printf("Could not find a Bin to accomodate Item %d of size %d\n\n", itm.id, itm.size);
}

void opti_priority(vector<struct bin> &v_bins, struct context &ctx)
{
        int p;
        vector<struct bin> v_bc = v_bins;

        /* detect items not schedulable */
        for (unsigned int i = 0; i < v_bins.size(); i++) {
                for (unsigned int j = 0; j < v_bins[i].v_itms.size(); j++) {
                        for (unsigned int k = 0; k < v_bins[i].v_itms[j].tc.v_tasks.size(); k++) {
                                if (v_bins[i].v_itms[j].tc.v_tasks[k].r > v_bins[i].v_itms[j].tc.v_tasks[k].t) {
                                        printf("Bin %d, Item %d task %d wcrt failed\n", 
                                                        v_bins[i].id, v_bins[i].v_itms[j].id, v_bins[i].v_itms[j].tc.v_tasks[k].id);
                                        /* save priority of unschedulable task */
                                        p = v_bins[i].v_itms[j].tc.v_tasks[k].p;

                                        for (unsigned int l = 0; l < v_bins[i].v_itms.size(); l++) {
                                                /* do no iterate the unschedulable item */
                                                if (v_bins[i].v_itms[l].id == v_bins[i].v_itms[j].id)
                                                        continue;
                                                /* assign new priority */
                                                for (unsigned int m = 0; m < v_bins[i].v_itms[l].tc.v_tasks.size(); m++) {
                                                        v_bins[i].v_itms[l].tc.v_tasks[m].p = ++p;
                                                }
                                        }
                                }
                        }
                }
        }
        sched_analysis(v_bins, ctx);
}
