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

static void _reassignment(struct bin &b, unsigned int &itm_id, int &p)
{
        int flag;
        struct bin b_tmp;

        b_tmp = b;
        flag = -1;

        /* starting new p */
        p = p + 1;
        printf("Updating Priorities... with p: %d\n", p);

        /* assign new priorities */
        for (unsigned int i = 0; i < b_tmp.v_tasks.size(); i++) {
                if (b_tmp.v_tasks[i].idx.itm_idx == (int)itm_id)
                        continue;

                if (b_tmp.v_tasks[i].p < p) {
                        for (unsigned int j = 0; j < b_tmp.v_itms[itm_id].tc.v_tasks.size(); j++) {
                                if (b_tmp.v_itms[itm_id].tc.v_tasks[j].p == p) {
                                        printf("priority %d already assigned\n", p);
                                        p++;
                                }
                        }
                        b_tmp.v_tasks[i].p = p;
                        p = p + 1;
                        printf("tau %d p: %d\n", b_tmp.v_tasks[i].id, b_tmp.v_tasks[i].p);
                }
        }

        flag = wcrt(b_tmp.v_tasks);

        /* copy back response time and priorities to tc */
        copy_back_resp(b_tmp);
        copy_back_prio(b_tmp);

        if (flag == SCHED_OK) {
                b = b_tmp;
                b.flag = SCHED_OK;
                printf("Core %d SCHED_OK with new priority assignment\n\n", b_tmp.id);
                for (unsigned int i = 0; i < b_tmp.v_tasks.size(); i++) {
                        if (b_tmp.v_tasks[i].r > b_tmp.v_tasks[i].t) {
                                printf("p: %d tau.id: %d r: %d t: %d\n", 
                                                b_tmp.v_tasks[i].p, 
                                                b_tmp.v_tasks[i].id, 
                                                b_tmp.v_tasks[i].r, 
                                                b_tmp.v_tasks[i].t);
                                printf("ERR! wcrt should have failed\n");
                                exit(0);
                        }
                }
                return;

        } else if (flag == SCHED_FAILED) {
                b.flag = SCHED_FAILED;
                printf("Core %d SCHED_FAILED with new priority assignment\n\n", b_tmp.id);
                return;

        } else {
                printf("ERR! priority reassignment\n");
                exit(0);
        }
}

void reassignment(vector<struct bin> &v_bins, struct context &ctx)
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
                                        _reassignment(v_bins[i], j, p);
                                }
                        }
                }
        }
}

void displacement(vector<struct bin> &v_bins, struct context &ctx)
{
        int ret;
        int flag;
        int min;
        int tmp_min;
        int best_bin_id;
        vector<struct bin> v_bi;
        vector<struct item> v_it;

        flag = NO;
        best_bin_id = -1;

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

        /* compute_tc_u to tc if itm is a fragment */
        for (unsigned int i = 0; i < v_it.size(); i++) {
                v_it[i].tc.u = 0;
                compute_tc_u(v_it[i]);
        }

        /* find a schedulable bin that has enough space for the itm to fit */
        for (unsigned int i = 0; i < v_it.size(); i++) {
                /* create a vector bin for each item to test */
                min = C;
                v_bi.clear();
                for (unsigned int j = 0; j < v_bins.size(); j++) {
                        if (v_bins[j].flag == SCHED_OK && flag == YES && v_bins[j].cap_rem >= v_it[i].tc.u) {
                                /* add bin in v_bi and add itm to v_bi */
                                v_bi.push_back(v_bins[j]);
                                v_bi.back().v_itms.push_back(v_it[i]);
                                v_bi.back().v_tasks.clear();
                                printf("Found Core %-3d for TC %-3d\n", v_bins[j].id, v_it[i].id);

                                /* copy itm task to v_tasks of bin */
                                for (unsigned int k = 0; k < v_bi.back().v_itms.size(); k++)
                                        copy_tc_to_v_tasks(v_bi.back(), j, k);

                                /* assign unique priorities to each tasks */
                                assign_unique_prio(v_bi.back());

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

void swapping(vector<struct bin> &v_bins, struct context &ctx)
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
