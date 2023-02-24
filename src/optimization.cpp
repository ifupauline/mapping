#include "optimization.h"
#include "sched_analysis.h"
#include "print.h"

static int test_new_priorities(struct bin &b, unsigned int j, int p)
{
        vector<struct task> v_tasks;
        struct bin b_tmp = b;
        int flag = -1;

        /* starting new p */
        p = p + 1;
        printf("Updating Priorities... with p: %d\n", p);

        for (unsigned int l = 0; l < b_tmp.v_itms.size(); l++) {
                /* do not iterate the unschedulable item */
                if (b_tmp.v_itms[l].id == b_tmp.v_itms[j].id)
                        continue;
                /* assign new priority */
                for (unsigned int m = 0; m < b_tmp.v_itms[l].tc.v_tasks.size(); m++) {
                        b_tmp.v_itms[l].tc.v_tasks[m].p = p++;
                }
        }


        /* extract tasks from concerned bin*/
        for (unsigned int i = 0; i < b_tmp.v_itms.size(); i++) {
                for (unsigned int j = 0; j < b_tmp.v_itms[i].tc.v_tasks.size(); j++) {
                        v_tasks.push_back(b_tmp.v_itms[i].tc.v_tasks[j]);
                        printf("Bin %d Item %d task %d p:%d\n", 
                                        b_tmp.id, b_tmp.v_itms[i].id, 
                                        b_tmp.v_itms[i].tc.v_tasks[j].id, 
                                        b_tmp.v_itms[i].tc.v_tasks[j].p);
                }
        }

        flag = wcrt(v_tasks);
        if (flag == SCHED_OK) {
                printf("Bin %d SCHED_OK with new priority assignment\n\n", b_tmp.id);
                return YES;
        }

        if (flag == SCHED_FAILED) {
                printf("Bin %d SCHED_FAILED with new priority assignment\n\n", b_tmp.id);
                return NO;
        }
        printf("ERR test_new_priorities\n");
        exit(0);
        return -1;
}

static void priority_reassignment(struct bin &b, unsigned int &j, int &p)
{
        for (unsigned int l = 0; l < b.v_itms.size(); l++) {
                /* do not iterate the unschedulable item */
                if (b.v_itms[l].id == b.v_itms[j].id)
                        continue;
                /* assign new priority */
                for (unsigned int m = 0; m < b.v_itms[l].tc.v_tasks.size(); m++) {
                        b.v_itms[l].tc.v_tasks[m].p = p++;
                }
        }
}

void opti_priority(vector<struct bin> &v_bins, struct context &ctx)
{
        int p;
        int flag;

        /* detect items not schedulable */
        for (unsigned int i = 0; i < v_bins.size(); i++) {
                for (unsigned int j = 0; j < v_bins[i].v_itms.size(); j++) {
                        for (unsigned int k = 0; k < v_bins[i].v_itms[j].tc.v_tasks.size(); k++) {
                                if (v_bins[i].v_itms[j].tc.v_tasks[k].r > v_bins[i].v_itms[j].tc.v_tasks[k].t) {
                                        printf("Bin %d, Item %d task %d wcrt failed\n", 
                                                        v_bins[i].id, v_bins[i].v_itms[j].id, v_bins[i].v_itms[j].tc.v_tasks[k].id);

                                        /* save priority of unscheduled task */
                                        p = v_bins[i].v_itms[j].tc.v_tasks[k].p;

                                        flag = test_new_priorities(v_bins[i], j, p);
                                        if (flag == YES)
                                                priority_reassignment(v_bins[i], j, p);
                                }
                        }
                }
        }
        sched_analysis(v_bins, ctx);
}
