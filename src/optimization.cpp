#include "print.h"
#include "optimization.h"
#include "sched_analysis.h"

static int cmp_inc_id(const struct task &a, const struct task &b)
{
        return a.id < b.id;
}

static void sort_inc_id(vector<struct task> &v_tasks)
{
        sort(v_tasks.begin(), v_tasks.end(), cmp_inc_id);
}

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

static void _displacing(struct bin &bin, struct item &itm)
{

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

void displacement_optimization(vector<struct bin> &v_bins, struct context &ctx)
{
        int ret;
        int min;
        int tmp_min;
        int best_bin_id = -1;
        int flag;
        vector<struct item> v_it;

        flag = NO;

        /* take next unschedulable itm */
        for (unsigned int i = 0; i < v_bins.size(); i++) {
                for (unsigned int j = 0; j < v_bins[i].v_itms.size(); j++) {
                        for (unsigned int k = 0; k < v_bins[i].v_itms[j].tc.v_tasks.size(); k++) {
                                if (v_bins[i].v_itms[j].tc.v_tasks[k].r == -1) {
                                        v_it.push_back(v_bins[i].v_itms[j]);
                                        flag = YES;
                                        printf("Save tc %-3d\n", v_bins[i].v_itms[j].id);
                                        break;
                                }
                        }
                }
        }

        /* copy back u to tc if itm is a fragment */
        for (unsigned int i = 0; i < v_it.size(); i++) {
                v_it[i].tc.u = 0;
                for (unsigned int j = 0; j < v_it[i].tc.v_tasks.size(); j++) {
                        v_it[i].tc.u += v_it[i].tc.v_tasks[j].u;
                }
        }

        /* find a schedulable bin that has enough space for the itm to fit */
        for (unsigned int i = 0; i < v_it.size(); i++) {
                /* create a vector bin for each item to test */
                vector<struct bin> v_bi;
                min = C;
                for (unsigned int j = 0; j < v_bins.size(); j++) {
                        if (v_bins[j].flag == SCHED_OK && flag == YES && v_bins[j].cap_rem >= v_it[i].tc.u) {
                                /* add bin in v_bi and add itm to v_bi */
                                v_bi.push_back(v_bins[j]);
                                v_bi.back().v_itms.push_back(v_it[i]);
                                v_bi.back().v_tasks.clear();
                                printf("Found Core %-3d for TC %-3d\n", v_bins[j].id, v_it[i].id);

                                /* copy itm task to v_tasks of bin */
                                for (unsigned int k = 0; k < v_bi.back().v_itms.size(); k++) {
                                        sort_inc_id(v_bi.back().v_itms[k].tc.v_tasks);
                                        for (unsigned int l = 0; l < v_bi.back().v_itms[k].tc.v_tasks.size(); l++) {
                                                v_bi.back().v_tasks.push_back(v_bi.back().v_itms[k].tc.v_tasks[l]);
                                        }
                                }
                                /* sort tasks by id and assign priorities */
                                sort_inc_id(v_bi.back().v_tasks);

                                /* assign unique priorities to each tasks */
                                for (unsigned int i = 0; i < v_bi.back().v_tasks.size(); i++) {
                                        v_bi.back().v_tasks[i].p = i + 1;
                                }

                                /* test wcrt */
                                ret = wcrt(v_bi.back().v_tasks);
                                if (ret == SCHED_OK) {
                                        printf("Test WCRT Core %d for TC %d OK!\n", v_bins[j].id, v_it[i].id);
                                        /* store min cap_rem */
                                        tmp_min = v_bins[j].cap_rem - v_it[i].tc.u;
                                        if (tmp_min < min) {
                                                min = tmp_min;
                                                best_bin_id = v_bins[j].id;
                                        }
                                }
                                else if (ret == SCHED_FAILED) {
                                        printf("No Displacement of TC %d has been found!\n\n", v_it[i].id);
                                        /* test priority reassigment */
                                }
                        }
                }
                if (best_bin_id == -1 || min == C)
                        continue;
                else
                        printf("Best Core %d with min %d for TC: %d\n\n", best_bin_id, min, v_it[i].id);
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
