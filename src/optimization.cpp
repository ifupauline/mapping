#include <bits/stdc++.h>

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

        /* update tc load if itm is a fragment */
        for (unsigned int i = 0; i < v_fail_itms.size(); i++) {
                v_fail_itms[i].first.tc.u = 0;
                compute_tc_load(v_fail_itms[i].first);
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
                //printf("Core %d SCHED_FAILED with new priority assignment\n\n", 
                //                b_tmp.id);
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
        high_p = INT_MAX;
        bin_idx = 0;
        itm_idx = 0;
        min = C;
        tmp_min = 0;
        is_found = NO;

        /* copy itm task to v_tasks of bin */
        for (unsigned int i = 0; i < v_fail_bins.size(); i++) {
                for (unsigned int j = 0; j < v_fail_bins[i].v_itms.size(); j++)
                        copy_tc_to_v_tasks_with_pos(v_fail_bins[i], i, j);
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

static void _reassign_by_bin_id(vector<struct bin> &v_bins, int bin_id)
{
        int p;

        p = -1;
        /* detect bin not schedulable */
        for (unsigned int i = 0; i < v_bins.size(); i++) {
                if (v_bins[i].flag == SCHED_FAILED && v_bins[i].id == bin_id) {
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
        fail_itm.second = 0;

        /* take next unschedulable itm */
        _store_unsched_itms(v_bins, v_fail_itms, flag);

        printf("\n");

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

static int _search_for_swap(vector<struct bin> &v_bins, 
                pair<struct item, int> fail_src_itm, 
                pair<struct item, int> fail_dst_itm)
{
        int flag_dst_src;
        int flag_src_dst;

        flag_src_dst = NO;
        flag_dst_src = NO;

        for (unsigned int i = 0; i < v_bins.size(); i++) {
                /* test src -> dst */
                if (v_bins[i].id == fail_src_itm.second)
                        continue;

                if (v_bins[i].id == fail_dst_itm.second) {
                        if (v_bins[i].cap_rem + fail_dst_itm.first.size >= fail_src_itm.first.size) {
                                flag_src_dst = YES;
                                break;
                        }
                }
        }

        for (unsigned int i = 0; i < v_bins.size(); i++) {
                if (v_bins[i].id == fail_dst_itm.second)
                        continue;

                /* test dst -> src */
                if (v_bins[i].id == fail_src_itm.second) {
                        if (v_bins[i].cap_rem + fail_src_itm.first.size >= fail_dst_itm.first.size) {
                                flag_dst_src = YES;
                                break;
                        }
                }
        }

        if (flag_src_dst == YES && flag_dst_src == YES) {
                //printf("Found swap for src TC %d of size %d from Core %d and dst TC %d of size %d from Core %d\n", 
                //                fail_src_itm.first.id, fail_src_itm.first.size, fail_src_itm.second,
                //                fail_dst_itm.first.id, fail_dst_itm.first.size, fail_dst_itm.second);
                return YES;
        }
        return NO;
}

void _swap(vector<struct bin> &v_bins, int src_tc_id, int dst_tc_id, 
                int src_bin_id, int dst_bin_id)
{
        int bin_idx;
        int flag_src;
        int flag_dst;
        struct item src_tc;
        struct item dst_tc;
        vector<struct bin> tmp_v_bins;

        tmp_v_bins = v_bins;
        flag_src = NO;
        flag_dst = NO;
        bin_idx = -1;

        /* save src_tc and dst_tc */
        retrieve_tc_by_id(tmp_v_bins, src_tc , src_tc_id);
        retrieve_tc_by_id(tmp_v_bins, dst_tc , dst_tc_id);

        /* remove src_tc from src_bin */
        for (unsigned int i = 0; i < tmp_v_bins.size(); i++) {
                if (tmp_v_bins[i].id == src_bin_id) {
                        for (unsigned int j = 0; j < tmp_v_bins[i].v_itms.size(); j++) {
                                if (tmp_v_bins[i].v_itms[j].id == src_tc_id) {
                                        tmp_v_bins[i].v_itms.erase(tmp_v_bins[i].v_itms.begin() + j);
                                        compute_bin_cap_rem(tmp_v_bins[i]);
                                        bin_idx = i;
                                }
                        }
                }
        }

        /* remove dst_tc from dst_bin */
        for (unsigned int i = 0; i < tmp_v_bins.size(); i++) {
                if (tmp_v_bins[i].id == dst_bin_id) {
                        for (unsigned int j = 0; j < tmp_v_bins[i].v_itms.size(); j++) {
                                if (tmp_v_bins[i].v_itms[j].id == dst_tc_id) {
                                        tmp_v_bins[i].v_itms.erase(tmp_v_bins[i].v_itms.begin() + j);
                                        compute_bin_cap_rem(tmp_v_bins[i]);
                                        bin_idx = i;
                                }
                        }
                }
        }

        /* insert dst_tc to src_bin */
        for (unsigned int i = 0; i < tmp_v_bins.size(); i++) {
                if (tmp_v_bins[i].id == src_bin_id) {
                        tmp_v_bins[i].v_itms.push_back(dst_tc);
                        bin_idx = i;

                        /* copy itm task to v_tasks of bin */
                        tmp_v_bins[i].v_tasks.clear();
                        for (unsigned int j = 0; j < tmp_v_bins[i].v_itms.size(); j++)
                                copy_tc_to_v_tasks_with_pos(tmp_v_bins[i], bin_idx, j);

                        assign_unique_priorities(tmp_v_bins[i]);
                        copy_back_prio_to_tc(tmp_v_bins[i]);
                        wcrt_bin(tmp_v_bins[i], bin_idx);

                        /* try to reassign priority */
                        if (tmp_v_bins[i].flag == SCHED_FAILED)
                                _reassign_by_bin_id(tmp_v_bins, src_bin_id);

                        if (tmp_v_bins[i].flag == SCHED_OK)
                                flag_src = YES;
                }
        }

        /* insert src_tc to dst_bin */
        for (unsigned int i = 0; i < tmp_v_bins.size(); i++) {
                if (tmp_v_bins[i].id == dst_bin_id) {
                        tmp_v_bins[i].v_itms.push_back(src_tc);
                        bin_idx = i;

                        /* copy itm task to v_tasks of bin */
                        tmp_v_bins[i].v_tasks.clear();
                        for (unsigned int j = 0; j < tmp_v_bins[i].v_itms.size(); j++)
                                copy_tc_to_v_tasks_with_pos(tmp_v_bins[i], bin_idx, j);

                        assign_unique_priorities(tmp_v_bins[i]);
                        copy_back_prio_to_tc(tmp_v_bins[i]);
                        wcrt_bin(tmp_v_bins[i], bin_idx);

                        /* try to reassign priority */
                        if (tmp_v_bins[i].flag == SCHED_FAILED)
                                _reassign_by_bin_id(tmp_v_bins, src_bin_id);

                        if (tmp_v_bins[i].flag == SCHED_OK)
                                flag_dst = YES;
                }
        }
        if (flag_src == YES && flag_dst == YES) {
                v_bins.clear();
                v_bins = tmp_v_bins;
                printf("<----Swap between src TC %d and dst TC %d Succeeded!---->\n", 
                                src_tc_id, dst_tc_id);
        }
}

void swapping(vector<struct bin> &v_bins)
{
        int flag;
        int src_tc_id;
        int dst_tc_id;
        int src_bin_id;
        int dst_bin_id;
        vector<pair<struct item, int>> v_fail_itms;

        flag = NO;
        src_bin_id = 0;
        dst_bin_id = 0;
        src_tc_id = 0;
        dst_tc_id = 0;

        /* store fail_bins */
        _store_unsched_itms(v_bins, v_fail_itms, flag);

        /* iterate over unsched itms */
        for (unsigned int i = 0; i < v_fail_itms.size(); i++) {
                for (unsigned int j = 0; j < v_fail_itms.size(); j++) {
                        /* skip itms in same bin */
                        if (v_fail_itms[j].second == v_fail_itms[i].second)
                                continue;

                        /* search if swap possible */
                        flag = _search_for_swap(v_bins, v_fail_itms[i], v_fail_itms[j]);

                        if (flag == YES) {
                                /* store src and dst bin */
                                src_bin_id = v_fail_itms[i].second;
                                dst_bin_id = v_fail_itms[j].second;
                                src_tc_id = v_fail_itms[i].first.id;
                                dst_tc_id = v_fail_itms[j].first.id;

                                /* swap */
                                _swap(v_bins, src_tc_id, dst_tc_id, src_bin_id, dst_bin_id);
                        }
                }
        }
}
