#include "print.h"
#include "optimization.h"
#include "sched_analysis.h"

static void _swapping(vector<struct bin> &v_bins, int src_bin_id, int src_itm_id, 
                int dst_bin_id, int dst_itm_id)
{
        int flag_src;
        int flag_dst;
        struct item src_itm;
        struct item dst_itm;

        flag_src = NO;
        flag_dst = NO;

        /* save src itm and remove original one */
        for (unsigned int i = 0; i < v_bins.size(); i++) {
                if (v_bins[i].id == src_bin_id) {
                        for (unsigned int j = 0; j < v_bins[i].v_itms.size(); j++) {
                                if (v_bins[i].v_itms[j].id == src_itm_id) {
                                        printf("Core %d task-chain %d\n", 
                                                        v_bins[i].id, v_bins[i].v_itms[j].id);
                                        src_itm = v_bins[i].v_itms[j];
                                        v_bins[i].v_itms.erase(v_bins[i].v_itms.begin() + j);
                                        flag_src = YES;
                                        break;
                                }
                        }
                }
        }

        /* save src itm and remove original one */
        for (unsigned int i = 0; i < v_bins.size(); i++) {
                if (v_bins[i].id == dst_bin_id) {
                        for (unsigned int j = 0; j < v_bins[i].v_itms.size(); j++) {
                                if (v_bins[i].v_itms[j].id == dst_itm_id) {
                                        printf("Core %d task-chain %d\n", 
                                                        v_bins[i].id, v_bins[i].v_itms[j].id);
                                        dst_itm = v_bins[i].v_itms[j];
                                        v_bins[i].v_itms.erase(v_bins[i].v_itms.begin() + j);
                                        flag_dst = YES;
                                        break;
                                }
                        }
                }
        }

        /* proceed to copy */
        if (flag_src == YES && flag_dst == YES) {

                /* copy src_itm to dst_bin */
                for (unsigned int i = 0; i < v_bins.size(); i++) {
                        if (v_bins[i].id == dst_bin_id) {
                                v_bins[i].v_itms.push_back(src_itm);
                                printf("Insert tc %d to Core %d\n", 
                                                src_itm.id, v_bins[i].id);
                        }

                        if (v_bins[i].id == src_bin_id) {
                                v_bins[i].v_itms.push_back(dst_itm);
                                printf("Insert tc %d to Core %d\n", 
                                                dst_itm.id, v_bins[i].id);
                        }
                }
        }
}

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

                                        /* save priority of unschedulable task */
                                        p = v_bins[i].v_itms[j].tc.v_tasks[k].p;
                                        priority_reassignment(v_bins[i], j, p);
                                }
                        }
                }
        }
}

void swapping_optimization(vector<struct bin> &v_bins, struct context &ctx)
{
        struct item src_itm;
        struct item dst_itm;
        vector<struct bin> t_v_bins;

        /* save unschedulable bins in temporary vector */
        for (unsigned int i = 0; i < v_bins.size(); i++) {
                if (v_bins[i].flag == SCHED_FAILED) {
                        t_v_bins.push_back(v_bins[i]);
                }
        }
        //print_cores(t_v_bins, ctx);

        /* take next unschedulable itm */
        for (unsigned int i = 0; i < t_v_bins.size(); i++) {
                for (unsigned int j = 0; j < t_v_bins[i].v_itms.size(); j++) {
                        for (unsigned int k = 0; k < t_v_bins[i].v_itms[j].tc.v_tasks.size(); k++) {
                                if (t_v_bins[i].v_itms[j].tc.v_tasks[k].r == -1) {
                                        src_itm = t_v_bins[i].v_itms[j];
                                }
                        }
                }
        }
}
