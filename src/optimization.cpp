#include "optimization.h"
#include "sched_analysis.h"

static void _store_unsched_items(vector<struct bin> &v_bins, 
                vector<pair<struct item, int>> &v_it, int &flag)
{
        pair<struct item, int> p;

        /* take next unschedulable itm */
        for (unsigned int i = 0; i < v_bins.size(); i++) {
                for (unsigned int j = 0; j < v_bins[i].v_itms.size(); j++) {
                        for (unsigned int k = 0; k < v_bins[i].v_itms[j].tc.v_tasks.size(); k++) {
                                if (v_bins[i].v_itms[j].tc.v_tasks[k].r == -1) {
                                        p.first = {0};
                                        p.second = 0;
                                        p.first = v_bins[i].v_itms[j];
                                        p.second = v_bins[i].id;
                                        v_it.push_back(p);
                                        flag = YES;
                                        printf("Add task-chain %-3d from Core %d\n", 
                                                        v_bins[i].v_itms[j].id,
                                                        v_bins[i].id);
                                        break;
                                }
                        }
                }
        }
}

static void _reassign(struct bin &b, int &p, int itm_idx)
{
        int flag;
        struct bin b_tmp;

        b_tmp = b;
        flag = -1;

        assign_new_priorities(b_tmp, p, itm_idx);

        /* test if schedulable */
        flag = wcrt(b_tmp.v_tasks);
        b_tmp.flag = flag;
        copy_back_resp_to_tc(b_tmp);
        copy_back_prio_to_tc(b_tmp);

        if (flag == SCHED_OK) {
                b = b_tmp;
                printf("Core %d SCHED_OK with new priority assignment\n\n", 
                                b_tmp.id);
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
                printf("Core %d SCHED_FAILED with new priority assignment\n\n", 
                                b_tmp.id);
                return;

        } else {
                printf("ERR! priority reassignment\n");
                exit(0);
        }
}

static int _search_dst_for_displace(vector<struct bin> &v_bins, 
                vector<pair<struct item, int>> &v_it, int &min, int item_idx)
{
        int tmp_min;
        int best_bin_id;

        /* copy itm task to v_tasks of bin */
        for (unsigned int i = 0; i < v_bins.size(); i++) {
                for (unsigned int j = 0; j < v_bins[i].v_itms.size(); j++)
                        copy_tc_to_v_tasks(v_bins[i], i, j);
        }

        for (unsigned int i = 0; i < v_bins.size(); i++) {
                /* test wcrt for dst bin */
                wcrt_bin(v_bins[i], i);
                if (v_bins[i].flag == SCHED_OK) {
                        printf("Test WCRT for task-chain %d to Core %d OK!\n", 
                                        v_it[item_idx].first.id, v_bins[i].id);
                        /* store min cap_rem */
                        tmp_min = v_bins[i].cap_rem - v_it[item_idx].first.tc.u;
                        if (tmp_min < min) {
                                min = tmp_min;
                                best_bin_id = v_bins[i].id;
                        }
                }
        }
        return best_bin_id;
}

static void _displace(vector<struct bin> &v_bins, pair<struct item, int> &p, 
                int dst_bin_id)
{
        /* remove itm from original bin */
        for (unsigned int i = 0; i < v_bins.size(); i++) {
                if (v_bins[i].id == p.second) {
                        for (unsigned int j = 0; j < v_bins[i].v_itms.size(); j++) {
                                if (v_bins[i].v_itms[j].id == p.first.id)
                                        delete_itm_by_id(v_bins, p.first.id);
                        }
                }
        }

        /* insert itm to new bin */
        for (unsigned int i = 0; i < v_bins.size(); i++) {
                if (v_bins[i].id == dst_bin_id) {
                        add_itm_by_id(v_bins, p.first, dst_bin_id);

                        if (v_bins[i].flag == SCHED_FAILED) {
                                printf("ERR! dst Displacement WCRT!\n");
                                exit(0);
                        }
                }
        }
        printf("\n");
}

void reassignment(vector<struct bin> &v_bins)
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
                                        _reassign(v_bins[i], p, j);
                                }
                        }
                }
        }
}

void displacement(vector<struct bin> &v_bins)
{
        int min;
        int flag;
        int best_bin_id;
        vector<struct bin> v_bi;
        pair<struct item, int> p;
        vector<pair<struct item, int>> v_it;

        flag = NO;
        best_bin_id = -1;

        /* take next unschedulable itm */
        _store_unsched_items(v_bins, v_it, flag);

        printf("\n");

        /* update tc load if itm is a fragment */
        for (unsigned int i = 0; i < v_it.size(); i++) {
                v_it[i].first.tc.u = 0;
                compute_tc_load(v_it[i].first);
        }

        /* find a schedulable bin that has enough space for the itm to fit */
        for (unsigned int i = 0; i < v_it.size(); i++) {
                /* create a vector bin for each item to test */
                min = C;
                v_bi.clear();
                for (unsigned int j = 0; j < v_bins.size(); j++) {
                        if (v_bins[j].flag == SCHED_OK && flag == YES && 
                                        v_bins[j].cap_rem >= v_it[i].first.tc.u) {
                                /* add bin in v_bi and add itm to v_bi */
                                v_bi.push_back(v_bins[j]);
                                v_bi.back().v_itms.push_back(v_it[i].first);
                                v_bi.back().v_tasks.clear();
                        }
                }

                /* test dst bins and save best bin */
                best_bin_id = _search_dst_for_displace(v_bi, v_it, min, i);

                /* if bin not found continue */
                if (best_bin_id == -1 || min == C)
                        continue;

                else {
                        /* displace */
                        p.first = {0};
                        p.second = 0;
                        p = v_it[i];
                        _displace(v_bins, p, best_bin_id);
                }
        }
}
