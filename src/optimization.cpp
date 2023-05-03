#include <bits/stdc++.h>

#include "print.h"
#include "sched_analysis.h"

#define MAX_DISP_COUNT 10
#define MAX_SWAP_COUNT 10

static void _store_unsched_itms(vector<struct bin> &v_bins, 
                vector<pair<struct item, int>> &v_fail_itms, int &flag)
{
        pair<struct item, int> fail_itm;

        fail_itm.first = {0};
        fail_itm.second = {0};

        /* take next unschedulable itm */
        for (unsigned int i = 0; i < v_bins.size(); i++) {
                for (unsigned int j = 0; j < v_bins[i].v_itms.size(); j++) {
                        for (unsigned int k = 0; k < v_bins[i].v_itms[j].v_tasks.size(); k++) {
                                if (v_bins[i].v_itms[j].v_tasks[k].r > v_bins[i].v_itms[j].v_tasks[k].t) {
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
                v_fail_itms[i].first.size = 0;
                compute_itm_load(v_fail_itms[i].first);
        }
}

static int _check_if_dual_frag(struct bin &b, int src_itm_id, 
                int src_bin_id, int src_itm_frag_id)
{
        for (unsigned int i = 0; i < b.v_itms.size(); i++) {
                if (src_itm_id == b.v_itms[i].id)
                        continue;

                if (src_itm_frag_id > -1 && src_itm_frag_id == b.v_itms[i].frag_id) {
                        printf("src tc %d Core %d is dual fragment with dst tc %d Core %d !\n", 
                                        src_itm_id, src_bin_id, b.v_itms[i].id, b.id);
                        return YES;
                }
        }
        return NO;
}



static int _search_for_displace(vector<struct bin> &v_fail_bins, 
                vector<pair<struct item, int>> &v_fail_itms, int item_idx, 
                struct bin &dst_b)
{
        int p;
        int high_p;
        int max;
        int tmp_max;
        int is_found;
        int bin_idx;
        int itm_idx;

        p = -1;
        high_p = INT_MAX;
        bin_idx = 0;
        itm_idx = 0;
        max = -1;
        tmp_max = 0;
        is_found = NO;

        /* copy itm task to v_tasks of bin */
        for (unsigned int i = 0; i < v_fail_bins.size(); i++) {
                for (unsigned int j = 0; j < v_fail_bins[i].v_itms.size(); j++)
                        copy_tc_to_v_tasks_with_pos(v_fail_bins[i], i, j);
        }

        for (unsigned int i = 0; i < v_fail_bins.size(); i++) {
                /* test wcrt for dst bin */
                priority_assignment(v_fail_bins[i]);
                wcrt_bin(v_fail_bins[i], i);
                if (v_fail_bins[i].flag == SCHED_OK) {
                        dst_b = v_fail_bins[i];
                        printf("Test WCRT for task-chain %d to Core %d OK!\n", 
                                        v_fail_itms[item_idx].first.id, v_fail_bins[i].id);
                        /* store max cap_rem */
                        tmp_max = v_fail_bins[i].load_rem - v_fail_itms[item_idx].first.size;
                        if (tmp_max > max)
                                max = tmp_max;
                        is_found = YES;
                }

                /* search for failed task with highest priority and _reassign */
                if (v_fail_bins[i].flag == SCHED_FAILED) {
                        for (unsigned int j = 0; j < v_fail_bins[i].v_itms.size(); j++) {
                                for (unsigned int k = 0; k < v_fail_bins[i].v_itms[j].v_tasks.size(); k++) {
                                        if (v_fail_bins[i].v_itms[j].v_tasks[k].r  == -1) {
                                                p = v_fail_bins[i].v_itms[j].v_tasks[k].p;
                                                if (p < high_p) {
                                                        high_p = p;
                                                        bin_idx = i;
                                                        itm_idx = j;
                                                }
                                        }
                                }
                        }
                        //reassign(v_fail_bins[bin_idx], high_p, itm_idx);
                        if (v_fail_bins[i].flag == SCHED_OK) {
                                dst_b = v_fail_bins[i];
                                /* store max cap_rem */
                                tmp_max = v_fail_bins[i].load_rem - v_fail_itms[item_idx].first.size;
                                if (tmp_max > max)
                                        max = tmp_max;
                                is_found = YES;
                        }
                }
        }
        return is_found;
}

static int _search_for_swap(vector<struct bin> &v_bins, 
                pair<struct item, int> fail_src_itm, 
                pair<struct item, int> fail_dst_itm)
{
        int flag_dst_src;
        int flag_src_dst;

        flag_src_dst = NO;
        flag_dst_src = NO;

        /* test src -> dst */
        for (unsigned int i = 0; i < v_bins.size(); i++) {
                if (v_bins[i].id == fail_src_itm.second)
                        continue;

                if (v_bins[i].id == fail_dst_itm.second) {
                        /* first check if dual frag present in dst bin */
                        int ret = _check_if_dual_frag(v_bins[i], 
                                        fail_src_itm.first.id, fail_src_itm.second, 
                                        fail_src_itm.first.frag_id);

                        /* TODO for now just skip */
                        if (ret == YES) {
                                flag_src_dst = NO;
                                break;
                        }

                        if (v_bins[i].load_rem + fail_dst_itm.first.size >= 
                                        fail_src_itm.first.size) {
                                flag_src_dst = YES;
                                break;
                        }
                }
        }

        /* test dst -> src */
        for (unsigned int i = 0; i < v_bins.size(); i++) {
                if (v_bins[i].id == fail_dst_itm.second)
                        continue;

                if (v_bins[i].id == fail_src_itm.second) {
                        /* first check if dual frag present in dst bin */
                        int ret = _check_if_dual_frag(v_bins[i], 
                                        fail_dst_itm.first.id, fail_dst_itm.second, 
                                        fail_dst_itm.first.frag_id);

                        /* TODO for now just skip */
                        if (ret == YES) {
                                flag_dst_src = NO;
                                break;
                        }


                        if (v_bins[i].load_rem + fail_src_itm.first.size >= 
                                        fail_dst_itm.first.size) {
                                flag_dst_src = YES;
                                break;
                        }
                }
        }

        if (flag_src_dst == YES && flag_dst_src == YES) {
                printf("\nFound Swap for src TC %d of size %d from Core %d and dst TC %d of size %d from Core %d\n", 
                                fail_src_itm.first.id, fail_src_itm.first.size, 
                                fail_src_itm.second, fail_dst_itm.first.id, 
                                fail_dst_itm.first.size, fail_dst_itm.second);
                return YES;
        }
        return NO;
}

static int _swap(vector<struct bin> &v_bins, int src_tc_id, int dst_tc_id, 
                int src_bin_id, int dst_bin_id, 
                pair<struct item, int> &src_itm, pair<struct item, int> &dst_itm)
{
        struct item src_tc;
        struct item dst_tc;

        struct bin src_bin;
        struct bin dst_bin;

        int src_bin_idx;
        int dst_bin_idx;

        src_tc = retrieve_tc_by_id(v_bins, src_tc_id);
        dst_tc = retrieve_tc_by_id(v_bins, dst_tc_id);

        src_bin = retrieve_core_by_id(v_bins, src_bin_id);
        dst_bin = retrieve_core_by_id(v_bins, dst_bin_id);

        src_bin_idx = get_bin_idx_by_id(v_bins, src_bin_id);
        dst_bin_idx = get_bin_idx_by_id(v_bins, dst_bin_id);

        /* remove  */
        delete_itm_by_id(src_bin, src_tc_id);
        delete_itm_by_id(dst_bin, dst_tc_id);

        /* insert  */
        insert_itm_to_core(src_bin, dst_tc);
        insert_itm_to_core(dst_bin, src_tc);

        /* test wcrt src_bin */
        src_bin.v_tasks.clear();
        for (unsigned int j = 0; j < src_bin.v_itms.size(); j++)
                copy_tc_to_v_tasks_with_pos(src_bin, src_bin_idx, j);

        priority_assignment(src_bin);
        copy_back_prio_to_tc(src_bin);
        wcrt_bin(src_bin, src_bin_idx);

        /* test wcrt dst_bin */
        dst_bin.v_tasks.clear();
        for (unsigned int j = 0; j < dst_bin.v_itms.size(); j++)
                copy_tc_to_v_tasks_with_pos(dst_bin, dst_bin_idx, j);

        priority_assignment(dst_bin);
        copy_back_prio_to_tc(dst_bin);
        wcrt_bin(dst_bin, dst_bin_idx);

        if (src_bin.flag == SCHED_OK || dst_bin.flag == SCHED_OK) {
                v_bins[src_bin_idx] = src_bin;
                v_bins[dst_bin_idx] = dst_bin;
                src_itm.second = dst_bin.id;
                dst_itm.second = src_bin.id;
                src_itm.first.swap_count++;
                dst_itm.first.swap_count++;
                printf("<----Swap between src TC %d and dst TC %d Succeeded!---->\n", 
                                src_tc_id, dst_tc_id);
                return YES;

        } else {
                printf("Could not Swap src TC %d and dst TC %d\n\n", 
                                src_tc_id, dst_tc_id);
                return NO;
        }
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
                                        delete_itm_by_id(v_bins[i], fail_itm.first.id);

                                        priority_assignment(v_bins[i]);
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
                                printf("Core %d flag %d\n", 
                                                v_bins[i].id, v_bins[i].flag);
                                exit(0);
                        }
                }
        }
        printf("\n");
}

void displacement(vector<struct bin> &v_bins)
{
        int ret;
        int flag;
        int state;
        int is_found;
        struct bin dst_b;
        pair<struct item, int> fail_itm;
        vector<struct bin> v_fail_bins;
        vector<pair<struct item, int>> v_fail_itms;

        ret = NO;
        flag = NO;
        state = NO;
        is_found = NO;
        dst_b = {0};
        fail_itm.first = {0};
        fail_itm.second = 0;

        /* take next unschedulable itm */
        _store_unsched_itms(v_bins, v_fail_itms, flag);

        printf("\n");

        while (1) {
                state = NO;
                /* find a schedulable bin that has enough space for the itm to fit */
                for (unsigned int i = 0; i < v_fail_itms.size(); i++) {
                        printf("Try to displace task-chain %-3d from Core %-3d\n", 
                                        v_fail_itms[i].first.id, v_fail_itms[i].second);
                        /* create a vector bin for each item to test */
                        v_fail_bins.clear();
                        for (unsigned int j = 0; j < v_bins.size(); j++) {
                                /* check if dst bin has the dual fragment of current itm */
                                ret = _check_if_dual_frag(v_bins[j], 
                                                v_fail_itms[i].first.id, v_fail_itms[i].second, 
                                                v_fail_itms[i].first.frag_id);

                                /* TODO for now just skip */
                                if (ret == YES)
                                        continue;

                                if (v_bins[j].flag == SCHED_OK && flag == YES && 
                                                v_bins[j].load_rem >= v_fail_itms[i].first.size) {
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

                        else if (is_found == YES) {
                                if (v_fail_itms[i].first.disp_count == MAX_DISP_COUNT)
                                        state = NO;
                                else {
                                        /* displace */
                                        fail_itm.first = {0};
                                        fail_itm.second = 0;
                                        fail_itm = v_fail_itms[i];
                                        v_fail_itms[i].first.disp_count++;
                                        _displace(v_bins, fail_itm, dst_b);
                                        is_found = NO;
                                        state = YES;
                                }
                        }
                }
                /* try priority reassignment for bins that lost a fail_itm */
                //reassignment(v_bins);

                if (state == NO)
                        break;
        }
}

void swapping(vector<struct bin> &v_bins)
{
        int ret;
        int flag;
        int state;
        int src_tc_id;
        int dst_tc_id;
        int src_bin_id;
        int dst_bin_id;
        vector<pair<struct item, int>> v_fail_itms;

        state = NO;
        flag = NO;
        src_bin_id = 0;
        dst_bin_id = 0;
        src_tc_id = 0;
        dst_tc_id = 0;

        /* store fail_bins */
        _store_unsched_itms(v_bins, v_fail_itms, flag);

        while (1) {
                state = NO;
                /* iterate over unsched itms */
                for (unsigned int i = 0; i < v_fail_itms.size(); i++) {
                        for (unsigned int j = 0; j < v_fail_itms.size(); j++) {
                                /* skip itms in same bin */
                                if (v_fail_itms[j].second == v_fail_itms[i].second)
                                        continue;

                                /* if tc has been moved too many times skip */
                                if (v_fail_itms[i].first.swap_count > MAX_SWAP_COUNT || 
                                                v_fail_itms[j].first.swap_count > MAX_SWAP_COUNT)
                                        continue;

                                /* search if swap is possible */
                                flag = _search_for_swap(v_bins, 
                                                v_fail_itms[i], v_fail_itms[j]);

                                if (flag == YES) {
                                        /* store src and dst bin */
                                        src_bin_id = v_fail_itms[i].second;
                                        dst_bin_id = v_fail_itms[j].second;
                                        src_tc_id = v_fail_itms[i].first.id;
                                        dst_tc_id = v_fail_itms[j].first.id;

                                        /* swap */
                                        ret = _swap(v_bins, src_tc_id, dst_tc_id, 
                                                        src_bin_id, dst_bin_id, v_fail_itms[i], v_fail_itms[j]);
                                        if (ret == YES)
                                                state = YES;
                                }
                        }
                }
                if (state == NO)
                        break;
        }
}
