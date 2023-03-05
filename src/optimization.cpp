#include "sched_analysis.h"

static int _search_unsched_task(vector<struct task> &v_tasks)
{
        int high_p;
        int flag = NO;
        vector<int> v_p;

        high_p = -1;

        for (unsigned int i = 0; i < v_tasks.size(); i++) {
                if (v_tasks[i].r  == -1) {
                        v_p.push_back(v_tasks[i].p);
                        flag = YES;
                }
        }

        if (flag == NO)
                return -1;

        else if (flag == YES) {
                /* look for unched task with highest priority */
                for (unsigned int i = 0; i < v_p.size(); i++) {
                        high_p = v_p[i];
                        if (high_p > v_p[i + 1])
                                high_p = v_p[i + 1];
                }
                return high_p;
        }
        return -1;
}

static void _store_unsched_itms(vector<struct bin> &v_bins, 
                vector<pair<struct item, int>> &v_fail_itms, int &flag)
{
        pair<struct item, int> fail_itm;

        fail_itm.first = {0};
        fail_itm.second = {0};

        /* take next unschedulable itm */
        for (unsigned int i = 0; i < v_bins.size(); i++) {
                for (unsigned int j = 0; j < v_bins[i].v_itms.size(); j++) {
                        for (unsigned int k = 0; k < v_bins[i].v_itms[j].tc.v_tasks.size(); k++) {
                                if (v_bins[i].v_itms[j].tc.v_tasks[k].r == -1) {
                                        fail_itm.first = {0};
                                        fail_itm.second = 0;
                                        fail_itm.first = v_bins[i].v_itms[j];
                                        fail_itm.second = v_bins[i].id;
                                        v_fail_itms.push_back(fail_itm);
                                        flag = YES;

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

        flag = -1;
        b_tmp = b;

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

static int _search_for_displace(vector<struct bin> &v_fail_bins, 
                vector<pair<struct item, int>> &v_fail_itms, int item_idx, 
                struct bin &dst_b)
{
        int p;
        int high_p;
        int min;
        int tmp_min;
        int is_found;
        int bin_idx;
        int itm_idx;

        p = -1;
        high_p = 1000;
        bin_idx = 0;
        itm_idx = 0;
        min = C;
        tmp_min = 0;
        is_found = NO;

        /* copy itm task to v_tasks of bin */
        for (unsigned int i = 0; i < v_fail_bins.size(); i++) {
                for (unsigned int j = 0; j < v_fail_bins[i].v_itms.size(); j++)
                        copy_tc_to_v_tasks(v_fail_bins[i], i, j);
        }

        for (unsigned int i = 0; i < v_fail_bins.size(); i++) {
                /* test wcrt for dst bin */
                assign_unique_priorities(v_fail_bins[i]);
                wcrt_bin(v_fail_bins[i], i);
                if (v_fail_bins[i].flag == SCHED_OK) {
                        printf("Test WCRT for task-chain %d to Core %d OK!\n", 
                                        v_fail_itms[item_idx].first.id, v_fail_bins[i].id);
                        /* store min cap_rem */
                        tmp_min = v_fail_bins[i].cap_rem - v_fail_itms[item_idx].first.tc.u;
                        if (tmp_min < min) {
                                min = tmp_min;
                                dst_b = v_fail_bins[i];
                        }
                        is_found = YES;
                }

                /* search for failed task with highest priority and _reassign */
                if (v_fail_bins[i].flag == SCHED_FAILED) {
                        for (unsigned int j = 0; j < v_fail_bins[i].v_itms.size(); j++) {
                                for (unsigned int k = 0; k < v_fail_bins[i].v_itms[j].tc.v_tasks.size(); k++) {
                                        if (v_fail_bins[i].v_itms[j].tc.v_tasks[k].r  == -1) {
                                                p = v_fail_bins[i].v_itms[j].tc.v_tasks[k].p;
                                                if (p < high_p) {
                                                        high_p = p;
                                                        bin_idx = i;
                                                        itm_idx = j;
                                                }
                                        }
                                }
                        }
                        _reassign(v_fail_bins[bin_idx], high_p, itm_idx);
                        if (v_fail_bins[i].flag == SCHED_OK) {
                                /* store min cap_rem */
                                tmp_min = v_fail_bins[i].cap_rem - v_fail_itms[item_idx].first.tc.u;
                                if (tmp_min < min) {
                                        min = tmp_min;
                                        dst_b = v_fail_bins[i];
                                }
                                is_found = YES;
                        }
                }
        }
        return is_found;
}

static void _displace(vector<struct bin> &v_bins, pair<struct item, 
                int> &fail_itm, struct bin &dst_b)
{
        /* remove fail_itm from its original bin */
        for (unsigned int i = 0; i < v_bins.size(); i++) {
                if (v_bins[i].id == fail_itm.second) {
                        for (unsigned int j = 0; j < v_bins[i].v_itms.size(); j++) {
                                if (v_bins[i].v_itms[j].id == fail_itm.first.id) {
                                        printf("Removing task-chain %d from Core %d\n", 
                                                        fail_itm.first.id, v_bins[i].id);
                                        delete_itm_by_id(v_bins, fail_itm.first.id);
                                        assign_unique_priorities(v_bins[i]);
                                        wcrt_bin(v_bins[i], i);
                                        if (v_bins[i].flag == SCHED_OK)
                                                printf("Core %d WCRT after removal of TC %d OK!\n", 
                                                                v_bins[i].id, fail_itm.first.id);

                                        if (v_bins[i].flag == SCHED_FAILED)
                                                printf("Core %d WCRT after removal of TC %d FAILED!\n", 
                                                                v_bins[i].id, fail_itm.first.id);
                                }
                        }
                }
        }

        /* insert fail_itm to target bin by bin copy */
        for (unsigned int i = 0; i < v_bins.size(); i++) {
                if (v_bins[i].id == dst_b.id) {
                        printf("Inserting task-chain %d in Core %d\n", 
                                        fail_itm.first.id, v_bins[i].id);
                        replace_bin_by_id(v_bins, dst_b);
                        wcrt_bin(v_bins[i], i);
                        if (v_bins[i].flag == SCHED_OK) {
                                printf("Core %d WCRT after insertion of TC %d OK!\n", 
                                                v_bins[i].id, fail_itm.first.id);
                        }

                        if (v_bins[i].flag == SCHED_FAILED) {
                                printf("ERR! dst Displacement WCRT of Core %d should have succeeded!\n", 
                                                v_bins[i].id);
                                printf("Core %d flag %d\n", v_bins[i].id, v_bins[i].flag);
                                exit(0);
                        }
                }
        }
        printf("\n");
}

void reassignment(vector<struct bin> &v_bins)
{
        int p;

        p = -1;
        /* detect bin not schedulable */
        for (unsigned int i = 0; i < v_bins.size(); i++) {
                if (v_bins[i].flag == SCHED_FAILED) {
                        for (unsigned int j = 0; j < v_bins[i].v_itms.size(); j++) {
                                p = _search_unsched_task(v_bins[i].v_itms[j].tc.v_tasks);

                                /* if no unscheduled task found go to next itm */
                                if (p == -1) 
                                        continue;
                                else
                                        _reassign(v_bins[i], p, j);
                        }
                }
        }
}

void displacement(vector<struct bin> &v_bins)
{
        int flag;
        int is_found;
        struct bin dst_b;
        pair<struct item, int> fail_itm;
        vector<struct bin> v_fail_bins;
        vector<pair<struct item, int>> v_fail_itms;

        flag = NO;
        is_found = NO;
        dst_b = {0};
        fail_itm.first = {0};

        /* take next unschedulable itm */
        _store_unsched_itms(v_bins, v_fail_itms, flag);

        printf("\n");

        /* update tc load if itm is a fragment */
        for (unsigned int i = 0; i < v_fail_itms.size(); i++) {
                v_fail_itms[i].first.tc.u = 0;
                compute_tc_load(v_fail_itms[i].first);
        }

        /* find a schedulable bin that has enough space for the itm to fit */
        for (unsigned int i = 0; i < v_fail_itms.size(); i++) {
                printf("Try to displace task-chain %-3d from Core %-3d\n", 
                                v_fail_itms[i].first.id, v_fail_itms[i].second);
                /* create a vector bin for each item to test */
                v_fail_bins.clear();
                for (unsigned int j = 0; j < v_bins.size(); j++) {
                        if (v_bins[j].flag == SCHED_OK && flag == YES && 
                                        v_bins[j].cap_rem >= v_fail_itms[i].first.tc.u) {
                                /* add bin in v_bi and add itm to v_bi */
                                v_fail_bins.push_back(v_bins[j]);
                                v_fail_bins.back().v_itms.push_back(v_fail_itms[i].first);
                                v_fail_bins.back().v_tasks.clear();
                        }
                }

                /* test dst bins and save best bin */
                is_found = _search_for_displace(v_fail_bins, v_fail_itms, i, dst_b);

                /* if bin not found continue */
                if (is_found == NO)
                        printf("Could not displace task-chain %-3d from Core %-3d\n\n", 
                                        v_fail_itms[i].first.id, v_fail_itms[i].second);

                else if (is_found == YES){
                        /* displace */
                        fail_itm.first = {0};
                        fail_itm.second = 0;
                        fail_itm = v_fail_itms[i];
                        _displace(v_bins, fail_itm, dst_b);
                        is_found = NO;
                }
        }

        /* try priority reassignment for bins that lost an fail_itm */
        reassignment(v_bins);
}
