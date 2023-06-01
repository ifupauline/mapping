#include "let.h"
#include "print.h"
#include "sched_analysis.h"

#define MAX_DISP_COUNT 5
#define MAX_SWAP_COUNT 5

static void _store_itms_disp(vector<struct bin> &v_bins, 
                vector<pair<struct item, int>> &v_itms, int &flag)
{
        pair<struct item, int> itm;

        itm.first = {0};
        itm.second = {0};

        /* take next unschedulable itm */
        for (unsigned int i = 0; i < v_bins.size(); i++) {
                if (v_bins[i].flag == SCHED_OK)
                        continue;
                for (unsigned int j = 0; j < v_bins[i].v_itms.size(); j++) {
                        /* skip LET */
                        if (v_bins[i].v_itms[j].is_let == YES)
                                continue;
                        itm.first = {0};
                        itm.second = 0;
                        itm.first = v_bins[i].v_itms[j];
                        itm.second = v_bins[i].id;
                        v_itms.push_back(itm);
                        flag = YES;
                }
        }

        /* update tc load if itm is a fragment */
        for (unsigned int i = 0; i < v_itms.size(); i++) {
                v_itms[i].first.size = 0;
                compute_itm_load(v_itms[i].first);
        }
}

static void _store_itms_swap(vector<struct bin> &v_bins, 
                vector<pair<struct item, int>> &v_itms, int &flag)
{
        pair<struct item, int> itm;

        itm.first = {0};
        itm.second = {0};

        /* take next unschedulable itm */
        for (unsigned int i = 0; i < v_bins.size(); i++) {
                for (unsigned int j = 0; j < v_bins[i].v_itms.size(); j++) {
                        /* skip LET */
                        if (v_bins[i].v_itms[j].is_let == YES)
                                continue;

                        itm.first = {0};
                        itm.second = 0;
                        itm.first = v_bins[i].v_itms[j];
                        itm.second = v_bins[i].id;
                        v_itms.push_back(itm);
                        flag = YES;
                }
        }

        /* update tc load if itm is a fragment */
        for (unsigned int i = 0; i < v_itms.size(); i++) {
                v_itms[i].first.size = 0;
                compute_itm_load(v_itms[i].first);
        }
}

static int _search_for_displace(vector<struct bin> &v_dst_bins, int item_id, 
                int item_idx, struct bin &dst_b)
{
        int max;
        int tmp_max;
        int is_found;

        max = 0;
        tmp_max = 0;
        is_found = NO;

        /* copy itm task to v_tasks of bin */
        copy_v_tc_to_v_tasks_with_pos(v_dst_bins);

        for (unsigned int i = 0; i < v_dst_bins.size(); i++) {
                /* test wcrt for dst bin */
                priority_assignment(v_dst_bins[i]);
                if (v_dst_bins[i].flag == SCHED_OK) {

                        printf("Test WCRT for TC %d to Core %d OK!\n", 
                                        item_id, v_dst_bins[i].id);
                        /* store max cap_rem */
                        tmp_max = v_dst_bins[i].load_rem;
                        if (tmp_max < 0) {
                                printf("ERR! disp tmp_max: %d\n", tmp_max);
                                exit(0);
                        }

                        if (tmp_max > max)  {
                                max = tmp_max;
                                dst_b = v_dst_bins[i];
                                is_found = YES;
                        }
                }
        }
        return is_found;
}

static int _search_for_swap(vector<struct bin> &v_bins, 
                pair<struct item, int> &src_itm, 
                pair<struct item, int> &dst_itm, 
                struct bin &dst_bin, struct bin &src_bin)
{
        int gcd;
        int load;
        int dst_flag;
        int src_flag;
        int dst_flag_cap;
        int src_flag_cap;

        gcd = 0;
        load = 0;
        dst_bin = {0};
        src_bin = {0};

        dst_flag = NO;
        src_flag = NO;
        dst_flag_cap = NO;
        src_flag_cap = NO;

        /* test src -> dst */
        for (unsigned int i = 0; i < v_bins.size(); i++) {
                if (v_bins[i].id == dst_itm.second) {
                        if (v_bins[i].flag == SCHED_OK)
                                dst_flag = YES;

                        /* check if fit */
                        dst_bin = v_bins[i];
                        delete_itm_by_id(dst_bin, dst_itm.first.id, dst_itm.first.tc_idx);
                        load = check_if_fit_itm(dst_bin, src_itm.first, gcd);

                        if (load <= dst_bin.phi) {
                                add_itm_to_bin(dst_bin, src_itm.first, load, gcd);
                                priority_assignment(dst_bin);
                                dst_flag_cap = YES;
                        } 
                }
        }

        /* test dst -> src */
        for (unsigned int i = 0; i < v_bins.size(); i++) {
                if (v_bins[i].id == src_itm.second) {
                        if (v_bins[i].flag == SCHED_OK)
                                src_flag = YES;

                        gcd = 0;
                        load = 0;

                        /* check if fit */
                        src_bin = v_bins[i];
                        delete_itm_by_id(src_bin, src_itm.first.id, src_itm.first.tc_idx);
                        load = check_if_fit_itm(src_bin, dst_itm.first, gcd);

                        if (load <= src_bin.phi) {
                                add_itm_to_bin(src_bin, dst_itm.first, load, gcd);
                                priority_assignment(src_bin);
                                src_flag_cap = YES;
                        } 
                }
        }

        if (dst_flag == YES && src_flag == YES)
                return NO;

        if (dst_flag_cap == YES && src_flag_cap == YES) {
                if (dst_flag == YES || src_flag == YES) {
                        if (dst_bin.flag == SCHED_OK && src_bin.flag == SCHED_OK) {
                                printf("\nFound Swap for src TC %d of size %d from Core %d and dst TC %d of size %d from Core %d\n", 
                                                src_itm.first.id, src_itm.first.size, 
                                                src_itm.second, dst_itm.first.id, 
                                                dst_itm.first.size, dst_itm.second);

                                dst_itm.second = src_bin.id;
                                src_itm.second = dst_bin.id;
                                dst_itm.first.swap_count++;
                                src_itm.first.swap_count++;
                                return YES;
                        }
                }

                if (dst_flag == NO && src_flag == NO) {
                        if (dst_bin.flag == SCHED_OK || src_bin.flag == SCHED_OK) {
                                printf("\nFound Swap for src TC %d of size %d from Core %d and dst TC %d of size %d from Core %d\n", 
                                                src_itm.first.id, src_itm.first.size, 
                                                src_itm.second, dst_itm.first.id, 
                                                dst_itm.first.size, dst_itm.second);

                                dst_itm.second = src_bin.id;
                                src_itm.second = dst_bin.id;
                                dst_itm.first.swap_count++;
                                src_itm.first.swap_count++;
                                return YES;
                        }
                }
        }
        return NO;
}

void _swap(vector<struct bin> &v_bins, struct bin &dst_bin, struct bin &src_bin)
{
        replace_bin_by_id(v_bins, dst_bin);
        replace_bin_by_id(v_bins, src_bin);
        printf("<---------------- SWAP SUCCESS ---------------->\n\n\n");
}

static void _displace(vector<struct bin> &v_bins, pair<struct item, 
                int> &itm, struct bin &dst_b)
{
        int src_bin_idx;
        int dst_bin_idx;

        src_bin_idx = 0;
        dst_bin_idx = 0;
        /* remove itm from its original bin */
        for (unsigned int i = 0; i < v_bins.size(); i++) {
                if (v_bins[i].id == itm.second) {
                        for (unsigned int j = 0; j < v_bins[i].v_itms.size(); j++) {
                                if (v_bins[i].v_itms[j].id == itm.first.id && 
                                                v_bins[i].v_itms[j].tc_idx == itm.first.tc_idx) {
                                        delete_itm_by_id(v_bins[i], itm.first.id, itm.first.tc_idx);
                                        copy_v_tc_to_v_tasks_with_pos(v_bins);
                                        priority_assignment(v_bins[i]);
                                        src_bin_idx = i;
                                }
                        }
                }
        }

        /* insert itm to target bin by bin copy */
        for (unsigned int i = 0; i < v_bins.size(); i++) {
                if (v_bins[i].id == dst_b.id) {
                        replace_bin_by_id(v_bins, dst_b);
                        copy_v_tc_to_v_tasks_with_pos(v_bins);
                        priority_assignment(v_bins[i]);
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
        printf("Removed TC %d idx: %d from Core %d\n", itm.first.id, 
                        itm.first.tc_idx, v_bins[src_bin_idx].id);
        printf("Inserted TC %d idx: %d in Core %d\n", itm.first.id, 
                        itm.first.tc_idx, v_bins[dst_bin_idx].id);
        printf("<---------------- DISPLACEMENT SUCCESS ---------------->\n\n\n");
}

void displacement(vector<struct bin> &v_bins)
{
        int gcd;
        int flag;
        int load;
        int state;
        int is_found;
        struct bin dst_b;
        pair<struct item, int> itm;
        vector<struct bin> v_dst_bins;
        vector<pair<struct item, int>> v_itms;

        flag = NO;
        state = NO;
        is_found = NO;
        gcd = 0;
        load = 0;
        dst_b = {0};
        itm.first = {0};
        itm.second = 0;


        /* take next unschedulable itm */
        _store_itms_disp(v_bins, v_itms, flag);
        for (unsigned int i = 0; i < v_itms.size(); i++)
                printf("TC %-3d  tc_idx %d from unfeasible Core %-3d\n", 
                                v_itms[i].first.id, 
                                v_itms[i].first.tc_idx, 
                                v_itms[i].second);
        printf("\n");

        if (flag == NO)
                return;

        while (1) {
                state = NO;
                /* find a schedulable bin that has enough space for the itm to fit */
                for (unsigned int i = 0; i < v_itms.size(); i++) {
                        printf("Try to displace TC %-3d tc_idx %d from Core %-3d\n", 
                                        v_itms[i].first.id, 
                                        v_itms[i].first.tc_idx, 
                                        v_itms[i].second);
                        v_dst_bins.clear();
                        for (unsigned int j = 0; j < v_bins.size(); j++) {
                                if (v_bins[j].flag == SCHED_FAILED)
                                        continue;
                                if (v_itms[i].second == v_bins[j].id)
                                        continue;
                                /* if item moved too many times skip */
                                if (v_itms[i].first.disp_count == MAX_DISP_COUNT) 
                                        break;

                                /* color filter */
                                if (v_itms[i].first.color != v_bins[j].color && 
                                                v_itms[i].first.color != WHITE)
                                        continue;

                                /* search for dst bins that can accomodate itm */
                                load = check_if_fit_itm(v_bins[j], v_itms[i].first, gcd);

                                if (load <= v_bins[j].phi) {
                                        /* add itm to potential bin */
                                        v_dst_bins.push_back(v_bins[j]);
                                        add_itm_to_bin(v_dst_bins.back(), v_itms[i].first, load, gcd);
                                }
                        }
                        /* test dst bins and save best bin */
                        is_found = _search_for_displace(v_dst_bins, v_itms[i].first.id, i, dst_b);

                        /* if bin not found continue */
                        if (is_found == YES) {
                                /* displace */
                                itm.first = {0};
                                itm.second = dst_b.id;
                                itm = v_itms[i];
                                v_itms[i].first.disp_count++;
                                v_itms[i].second = dst_b.id;
                                _displace(v_bins, itm, dst_b);
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
        int src_color;
        int dst_color;
        struct bin dst_bin;
        struct bin src_bin;
        vector<pair<struct item, int>> v_itms;

        flag = NO;
        state = NO;

        /* store fail_bins */
        _store_itms_swap(v_bins, v_itms, flag);
        for (unsigned int i = 0; i < v_itms.size(); i++)
                printf("TC %-3d from unfeasible Core %-3d\n", 
                                v_itms[i].first.id, v_itms[i].second);

        if (flag == NO)
                return;

        while (1) {
                state = NO;
                /* iterate over unsched itms */
                for (unsigned int i = 0; i < v_itms.size(); i++) {
                        for (unsigned int j = 0; j < v_itms.size(); j++) {
                                /* skip itms in same bin */
                                if (v_itms[i].second == v_itms[j].second)
                                        continue;

                                /* if tc has been moved too many times skip */
                                if (v_itms[i].first.swap_count > MAX_SWAP_COUNT || 
                                                v_itms[j].first.swap_count > MAX_SWAP_COUNT)
                                        continue;

                                /* none white */
                                if (v_itms[i].first.color != WHITE && v_itms[j].first.color != WHITE) {
                                        src_color = 0;
                                        dst_color = 0;
                                        src_color = retrieve_color_bin(v_bins, v_itms[i].second);
                                        dst_color = retrieve_color_bin(v_bins, v_itms[j].second);
                                        if (v_itms[i].first.color == src_color && v_itms[j].first.color == dst_color) {
                                                if (src_color == dst_color) {
                                                        printf("Trying to swap src TC %d tc_idx %d from Core %d with dst TC %d idx: %d from Core %d\n",
                                                                        v_itms[i].first.id, v_itms[i].first.tc_idx, v_itms[i].second,
                                                                        v_itms[j].first.id, v_itms[j].first.tc_idx, v_itms[j].second);

                                                        /* search if swap is possible */
                                                        dst_bin = {0};
                                                        src_bin = {0};
                                                        flag = NO;
                                                        flag = _search_for_swap(v_bins, v_itms[i], 
                                                                        v_itms[j], dst_bin, src_bin);

                                                        if (flag == YES) {
                                                                _swap(v_bins, dst_bin, src_bin);
                                                                state = YES;
                                                                break;
                                                        }
                                                }

                                        }

                                }

                                /* both white */
                                if (v_itms[i].first.color == WHITE && v_itms[j].first.color == WHITE) {
                                        printf("Trying to swap src TC %d tc_idx %d from Core %d with dst TC %d idx: %d from Core %d\n",
                                                        v_itms[i].first.id, v_itms[i].first.tc_idx, v_itms[i].second,
                                                        v_itms[j].first.id, v_itms[j].first.tc_idx, v_itms[j].second);

                                        /* search if swap is possible */
                                        dst_bin = {0};
                                        src_bin = {0};
                                        flag = NO;
                                        flag = _search_for_swap(v_bins, v_itms[i], 
                                                        v_itms[j], dst_bin, src_bin);

                                        if (flag == YES) {
                                                _swap(v_bins, dst_bin, src_bin);
                                                state = YES;
                                                break;
                                        }
                                }

                                /* src white dst not white */
                                if (v_itms[i].first.color == WHITE && v_itms[j].first.color != WHITE) {
                                        src_color = 0;
                                        dst_color = 0;
                                        src_color = retrieve_color_bin(v_bins, v_itms[i].second);
                                        dst_color = retrieve_color_bin(v_bins, v_itms[j].second);
                                        if (src_color == dst_color) {
                                                printf("Trying to swap src TC %d tc_idx %d from Core %d with dst TC %d idx: %d from Core %d\n",
                                                                v_itms[i].first.id, v_itms[i].first.tc_idx, v_itms[i].second,
                                                                v_itms[j].first.id, v_itms[j].first.tc_idx, v_itms[j].second);

                                                /* search if swap is possible */
                                                dst_bin = {0};
                                                src_bin = {0};
                                                flag = NO;
                                                flag = _search_for_swap(v_bins, v_itms[i], 
                                                                v_itms[j], dst_bin, src_bin);

                                                if (flag == YES) {
                                                        _swap(v_bins, dst_bin, src_bin);
                                                        state = YES;
                                                        break;
                                                }
                                        }
                                }

                                /* src not white dst white */
                                if (v_itms[i].first.color != WHITE && v_itms[j].first.color == WHITE) {
                                        src_color = 0;
                                        dst_color = 0;
                                        src_color = retrieve_color_bin(v_bins, v_itms[i].second);
                                        dst_color = retrieve_color_bin(v_bins, v_itms[j].second);
                                        if (src_color == dst_color) {
                                                printf("Trying to swap src TC %d tc_idx %d from Core %d with dst TC %d idx: %d from Core %d\n",
                                                                v_itms[i].first.id, v_itms[i].first.tc_idx, v_itms[i].second,
                                                                v_itms[j].first.id, v_itms[j].first.tc_idx, v_itms[j].second);

                                                /* search if swap is possible */
                                                dst_bin = {0};
                                                src_bin = {0};
                                                flag = NO;
                                                flag = _search_for_swap(v_bins, v_itms[i], 
                                                                v_itms[j], dst_bin, src_bin);

                                                if (flag == YES) {
                                                        _swap(v_bins, dst_bin, src_bin);
                                                        state = YES;
                                                        break;
                                                }
                                        }
                                }
                        }
                }
                if (state == NO)
                        break;
        }
}
