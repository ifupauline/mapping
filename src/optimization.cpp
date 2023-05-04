#include "let.h"
#include "print.h"
#include "sched_analysis.h"

#define MAX_DISP_COUNT 10
#define MAX_SWAP_COUNT 1

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
                        printf("src TC %d Core %d is dual fragment with dst TC %d Core %d !\n", 
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
        int max;
        int tmp_max;
        int is_found;

        max = -1;
        tmp_max = 0;
        is_found = NO;

        /* copy itm task to v_tasks of bin */
        copy_v_tc_to_v_tasks_with_pos(v_fail_bins);

        for (unsigned int i = 0; i < v_fail_bins.size(); i++) {
                /* test wcrt for dst bin */
                priority_assignment(v_fail_bins[i]);
                if (v_fail_bins[i].flag == SCHED_OK) {
                        dst_b = v_fail_bins[i];
                        printf("Test WCRT for TC %d to Core %d OK!\n", 
                                        v_fail_itms[item_idx].first.id, v_fail_bins[i].id);
                        /* store max cap_rem */
                        tmp_max = v_fail_bins[i].load_rem - v_fail_itms[item_idx].first.size;
                        if (tmp_max > max)
                                max = tmp_max;
                        is_found = YES;
                }
        }
        return is_found;
}

static int _search_for_swap(vector<struct bin> &v_bins, 
                pair<struct item, int> &fail_src_itm, 
                pair<struct item, int> &fail_dst_itm, 
                struct bin &dst_bin, struct bin &src_bin)
{
        int ret;
        int gcd;
        int load;
        vector<struct bin> tmp_v_bins;

        ret = 0;
        gcd = 0;
        load = 0;
        dst_bin = {0};
        src_bin = {0};

        /* test src -> dst */
        for (unsigned int i = 0; i < v_bins.size(); i++) {
                if (v_bins[i].id == fail_src_itm.second)
                        continue;

                if (v_bins[i].id == fail_dst_itm.second) {
                        /* first check if dual frag present in dst bin */
                        ret = _check_if_dual_frag(v_bins[i], 
                                        fail_src_itm.first.id, fail_src_itm.second, 
                                        fail_src_itm.first.frag_id);

                        /* TODO for now just skip */
                        if (ret == YES) {
                                break;
                        }

                        /* check if fit */
                        dst_bin = v_bins[i];
                        delete_itm_by_id(dst_bin, fail_dst_itm.first.id);
                        load = check_if_fit_itm(dst_bin, fail_src_itm.first, gcd);

                        if (load <= dst_bin.phi) {
                                printf("DST----------\n");
                                printf("TC %d can fit in Core %d\n", fail_src_itm.first.id, dst_bin.id);
                                add_itm_to_bin(dst_bin, fail_src_itm.first, load, gcd);

                                tmp_v_bins.clear();
                                tmp_v_bins = v_bins;
                                tmp_v_bins[i] = dst_bin;
                                copy_v_tc_to_v_tasks_with_pos(tmp_v_bins);
                                dst_bin = tmp_v_bins[i];

                                priority_assignment(dst_bin);
                        } 
                }
        }

        /* test dst -> src */
        for (unsigned int i = 0; i < v_bins.size(); i++) {
                if (v_bins[i].id == fail_dst_itm.second)
                        continue;

                if (v_bins[i].id == fail_src_itm.second) {
                        /* first check if dual frag present in dst bin */
                        ret = _check_if_dual_frag(v_bins[i], 
                                        fail_dst_itm.first.id, fail_dst_itm.second, 
                                        fail_dst_itm.first.frag_id);

                        /* TODO for now just skip */
                        if (ret == YES) {
                                break;
                        }

                        gcd = 0;
                        load = 0;

                        /* check if fit */
                        src_bin = v_bins[i];
                        delete_itm_by_id(src_bin, fail_src_itm.first.id);
                        load = check_if_fit_itm(src_bin, fail_dst_itm.first, gcd);

                        if (load <= src_bin.phi) {
                                printf("SRC----------\n");
                                printf("TC %d can fit in Core %d\n", fail_dst_itm.first.id, src_bin.id);
                                add_itm_to_bin(src_bin, fail_dst_itm.first, load, gcd);

                                tmp_v_bins.clear();
                                tmp_v_bins = v_bins;
                                tmp_v_bins[i] = src_bin;
                                copy_v_tc_to_v_tasks_with_pos(tmp_v_bins);
                                src_bin = tmp_v_bins[i];

                                priority_assignment(src_bin);
                        } 
                }
        }

        if (dst_bin.flag == SCHED_OK && src_bin.flag == SCHED_OK) {
                printf("\nFound Swap for src TC %d of size %d from Core %d and dst TC %d of size %d from Core %d\n", 
                                fail_src_itm.first.id, fail_src_itm.first.size, 
                                fail_src_itm.second, fail_dst_itm.first.id, 
                                fail_dst_itm.first.size, fail_dst_itm.second);

                fail_dst_itm.second = src_bin.id;
                fail_src_itm.second = dst_bin.id;
                fail_dst_itm.first.swap_count++;
                fail_src_itm.first.swap_count++;
                return YES;
        }
        return NO;
}

void _swap(vector<struct bin> &v_bins, struct bin &dst_bin, struct bin &src_bin)
{
        replace_bin_by_id(v_bins, dst_bin);
        replace_bin_by_id(v_bins, src_bin);
}

static void _displace(vector<struct bin> &v_bins, pair<struct item, 
                int> &fail_itm, struct bin &dst_b)
{
        int src_bin_idx;
        int dst_bin_idx;

        src_bin_idx = 0;
        dst_bin_idx = 0;
        /* remove fail_itm from its original bin */
        for (unsigned int i = 0; i < v_bins.size(); i++) {
                if (v_bins[i].id == fail_itm.second) {
                        for (unsigned int j = 0; j < v_bins[i].v_itms.size(); j++) {
                                if (v_bins[i].v_itms[j].id == fail_itm.first.id) {
                                        delete_itm_by_id(v_bins[i], fail_itm.first.id);
                                        copy_v_tc_to_v_tasks_with_pos(v_bins);
                                        priority_assignment(v_bins[i]);
                                        src_bin_idx = i;
                                }
                        }
                }
        }

        /* insert fail_itm to target bin by bin copy */
        for (unsigned int i = 0; i < v_bins.size(); i++) {
                if (v_bins[i].id == dst_b.id) {
                        replace_bin_by_id(v_bins, dst_b);
                        copy_v_tc_to_v_tasks_with_pos(v_bins);
                        priority_assignment(v_bins[i]);
                        fail_itm.second = v_bins[i].id; /* update bin id of itm */
                        dst_bin_idx = i;

                        if (v_bins[i].flag == SCHED_FAILED) {
                                printf("ERR! dst Displacement WCRT of Core %d should have succeeded!\n", 
                                                v_bins[i].id);
                                printf("Core %d flag %d\n", 
                                                v_bins[i].id, v_bins[i].flag);
                                exit(0);
                        }
                }
        }
        printf("Removed TC %d from Core %d\n", fail_itm.first.id, v_bins[src_bin_idx].id);
        printf("Inserted TC %d in Core %d\n", fail_itm.first.id, v_bins[dst_bin_idx].id);
        printf("<---------------- DISPLACEMENT SUCCESS ---------------->\n\n\n");
}

void displacement(vector<struct bin> &v_bins)
{
        int ret;
        int gcd;
        int flag;
        int load;
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
        load = 0;
        gcd = 0;
        dst_b = {0};
        fail_itm.first = {0};
        fail_itm.second = 0;

        /* take next unschedulable itm */
        _store_unsched_itms(v_bins, v_fail_itms, flag);
        for (unsigned int i = 0; i < v_fail_itms.size(); i++)
                printf("TC %-3d from Core %-3d unfeasible\n", 
                                v_fail_itms[i].first.id, v_fail_itms[i].second);
        printf("\n");

        while (1) {
                state = NO;
                /* find a schedulable bin that has enough space for the itm to fit */
                for (unsigned int i = 0; i < v_fail_itms.size(); i++) {
                        printf("Try to displace TC %-3d from Core %-3d\n", 
                                        v_fail_itms[i].first.id, v_fail_itms[i].second);
                        v_fail_bins.clear();
                        for (unsigned int j = 0; j < v_bins.size(); j++) {
                                /* if item moved too many times skip */
                                if (v_fail_itms[i].first.disp_count == MAX_DISP_COUNT)
                                        continue;

                                /* check if dst bin has the dual fragment of current itm */
                                ret = _check_if_dual_frag(v_bins[j], 
                                                v_fail_itms[i].first.id, v_fail_itms[i].second, 
                                                v_fail_itms[i].first.frag_id);

                                /* TODO for now do not allow same frags in same bin */
                                if (ret == YES)
                                        continue;

                                /* search for bins that can accomodate fail itm */
                                if (v_bins[j].flag == SCHED_OK && flag == YES && 
                                                /* make sure it is not a bin of a fail_itm which became feasible */
                                                v_bins[j].id != v_fail_itms[i].second) {

                                        /* check if itm fit */
                                        load = check_if_fit_itm(v_bins[j], v_fail_itms[i].first, gcd);

                                        if (load <= v_bins[j].phi) {
                                                /* add fail itm to potential bin */
                                                v_fail_bins.push_back(v_bins[j]);
                                                add_itm_to_bin(v_fail_bins.back(), v_fail_itms[i].first, load, gcd);
                                                /* test dst bins and save best bin */
                                                is_found = _search_for_displace(v_fail_bins, v_fail_itms, i, dst_b);
                                        }
                                }
                        }
                        /* if bin not found continue */
                        if (is_found == YES) {
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
                if (state == NO)
                        break;
        }
}

void swapping(vector<struct bin> &v_bins)
{
        int flag;
        int state;
        struct bin dst_bin;
        struct bin src_bin;
        vector<pair<struct item, int>> v_fail_itms;

        flag = NO;
        state = NO;

        /* store fail_bins */
        _store_unsched_itms(v_bins, v_fail_itms, flag);
        for (unsigned int i = 0; i < v_fail_itms.size(); i++)
                printf("TC %-3d from Core %-3d unfeasible\n", v_fail_itms[i].first.id, v_fail_itms[i].second);

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

                                printf("Trying to swap src TC %d from Core %d with dst TC %d from Core %d\n",
                                                v_fail_itms[i].first.id, v_fail_itms[i].second,
                                                v_fail_itms[j].first.id, v_fail_itms[j].second);

                                /* search if swap is possible */
                                flag = _search_for_swap(v_bins, v_fail_itms[i], 
                                                v_fail_itms[j], dst_bin, src_bin);

                                if (flag == YES) {
                                        /* swap */
                                        _swap(v_bins, dst_bin, src_bin);
                                        printf("<-------------------- SWAP SUCCEDED -------------------->\n\n\n");
                                        state = YES;
                                }
                        }
                }
                if (state == NO)
                        break;
        }
}
