#include "let.h"
#include "print.h"
#include "sched_analysis.h"

#define MAX_DISP_COUNT 5
#define MAX_SWAP_COUNT 5

static void _store_tcs_disp(vector<struct core> &v_cores, 
                vector<pair<struct tc, int>> &v_tcs, int &flag)
{
        pair<struct tc, int> tc;

        tc.first = {0};
        tc.second = {0};

        /* take next unschedulable tc */
        for (unsigned int i = 0; i < v_cores.size(); i++) {
                if (v_cores[i].flag == SCHED_OK)
                        continue;
                for (unsigned int j = 0; j < v_cores[i].v_tcs.size(); j++) {
                        /* skip LET */
                        if (v_cores[i].v_tcs[j].is_let == YES)
                                continue;
                        tc.first = {0};
                        tc.second = 0;
                        tc.first = v_cores[i].v_tcs[j];
                        tc.second = v_cores[i].id;
                        v_tcs.push_back(tc);
                        flag = YES;
                }
        }

        /* update tc load if tc is a fragment */
        for (unsigned int i = 0; i < v_tcs.size(); i++) {
                v_tcs[i].first.size = 0;
                cmp_tc_load(v_tcs[i].first);
        }
}

static void _store_tcs_swap(vector<struct core> &v_cores, 
                vector<pair<struct tc, int>> &v_tcs, int &flag)
{
        pair<struct tc, int> tc;

        tc.first = {0};
        tc.second = {0};

        /* take next unschedulable tc */
        for (unsigned int i = 0; i < v_cores.size(); i++) {
                for (unsigned int j = 0; j < v_cores[i].v_tcs.size(); j++) {
                        /* skip LET */
                        if (v_cores[i].v_tcs[j].is_let == YES)
                                continue;

                        tc.first = {0};
                        tc.second = 0;
                        tc.first = v_cores[i].v_tcs[j];
                        tc.second = v_cores[i].id;
                        v_tcs.push_back(tc);
                        flag = YES;
                }
        }

        /* update tc load if tc is a fragment */
        for (unsigned int i = 0; i < v_tcs.size(); i++) {
                v_tcs[i].first.size = 0;
                cmp_tc_load(v_tcs[i].first);
        }
}

static int _search_for_disp(vector<struct core> &v_dst_cores, int tc_id, 
                int tc_idx, struct core &dst_b)
{
        int max;
        int tmp_max;
        int is_found;

        max = 0;
        tmp_max = 0;
        is_found = NO;

        /* copy tc task to v_tasks of core */
        copy_v_tc_to_v_tasks_with_pos(v_dst_cores);

        for (unsigned int i = 0; i < v_dst_cores.size(); i++) {
                /* test wcrt for dst core */
                priority_assignment(v_dst_cores[i]);
                if (v_dst_cores[i].flag == SCHED_OK) {

                        printf("Test WCRT for TC %d to Core %d OK!\n", 
                                        tc_id, v_dst_cores[i].id);
                        /* store max cap_rem */
                        tmp_max = v_dst_cores[i].load_rem;
                        if (tmp_max < 0) {
                                printf("ERR! disp tmp_max: %d\n", tmp_max);
                                exit(0);
                        }

                        if (tmp_max > max)  {
                                max = tmp_max;
                                dst_b = v_dst_cores[i];
                                is_found = YES;
                        }
                }
        }
        return is_found;
}

static int _search_for_swap(vector<struct core> &v_cores, 
                pair<struct tc, int> &src_tc, 
                pair<struct tc, int> &dst_tc, 
                struct core &dst_core, struct core &src_core)
{
        int gcd;
        int load;
        int dst_flag;
        int src_flag;
        int dst_flag_cap;
        int src_flag_cap;

        gcd = 0;
        load = 0;
        dst_core = {0};
        src_core = {0};

        dst_flag = NO;
        src_flag = NO;
        dst_flag_cap = NO;
        src_flag_cap = NO;

        /* test src -> dst */
        for (unsigned int i = 0; i < v_cores.size(); i++) {
                if (v_cores[i].id == dst_tc.second) {
                        if (v_cores[i].flag == SCHED_OK)
                                dst_flag = YES;

                        /* check if fit */
                        dst_core = v_cores[i];
                        del_tc_by_id(dst_core, dst_tc.first.id, dst_tc.first.tc_idx);
                        load = check_if_fit_tc(dst_core, src_tc.first, gcd);

                        if (load <= dst_core.phi) {
                                add_tc_to_core(dst_core, src_tc.first, load, gcd);
                                priority_assignment(dst_core);
                                dst_flag_cap = YES;
                        } 
                }
        }

        /* test dst -> src */
        for (unsigned int i = 0; i < v_cores.size(); i++) {
                if (v_cores[i].id == src_tc.second) {
                        if (v_cores[i].flag == SCHED_OK)
                                src_flag = YES;

                        gcd = 0;
                        load = 0;

                        /* check if fit */
                        src_core = v_cores[i];
                        del_tc_by_id(src_core, src_tc.first.id, src_tc.first.tc_idx);
                        load = check_if_fit_tc(src_core, dst_tc.first, gcd);

                        if (load <= src_core.phi) {
                                add_tc_to_core(src_core, dst_tc.first, load, gcd);
                                priority_assignment(src_core);
                                src_flag_cap = YES;
                        } 
                }
        }

        if (dst_flag == YES && src_flag == YES)
                return NO;

        if (dst_flag_cap == YES && src_flag_cap == YES) {
                if (dst_flag == YES || src_flag == YES) {
                        if (dst_core.flag == SCHED_OK && src_core.flag == SCHED_OK) {
                                printf("\nFound Swap for src TC %d of size %d from Core %d and dst TC %d of size %d from Core %d\n", 
                                                src_tc.first.id, src_tc.first.size, 
                                                src_tc.second, dst_tc.first.id, 
                                                dst_tc.first.size, dst_tc.second);

                                dst_tc.second = src_core.id;
                                src_tc.second = dst_core.id;
                                dst_tc.first.swap_count++;
                                src_tc.first.swap_count++;
                                return YES;
                        }
                }

                if (dst_flag == NO && src_flag == NO) {
                        if (dst_core.flag == SCHED_OK || src_core.flag == SCHED_OK) {
                                printf("\nFound Swap for src TC %d of size %d from Core %d and dst TC %d of size %d from Core %d\n", 
                                                src_tc.first.id, src_tc.first.size, 
                                                src_tc.second, dst_tc.first.id, 
                                                dst_tc.first.size, dst_tc.second);

                                dst_tc.second = src_core.id;
                                src_tc.second = dst_core.id;
                                dst_tc.first.swap_count++;
                                src_tc.first.swap_count++;
                                return YES;
                        }
                }
        }
        return NO;
}

void _swap(vector<struct core> &v_cores, struct core &dst_core, struct core &src_core)
{
        rplc_core_by_id(v_cores, dst_core);
        rplc_core_by_id(v_cores, src_core);
        printf("<---------------- SWAP SUCCESS ---------------->\n\n\n");
}

static void _disp(vector<struct core> &v_cores, pair<struct tc, 
                int> &tc, struct core &dst_b)
{
        int src_core_idx;
        int dst_core_idx;

        src_core_idx = 0;
        dst_core_idx = 0;
        /* remove tc from its original core */
        for (unsigned int i = 0; i < v_cores.size(); i++) {
                if (v_cores[i].id == tc.second) {
                        for (unsigned int j = 0; j < v_cores[i].v_tcs.size(); j++) {
                                if (v_cores[i].v_tcs[j].id == tc.first.id && 
                                                v_cores[i].v_tcs[j].tc_idx == tc.first.tc_idx) {
                                        del_tc_by_id(v_cores[i], tc.first.id, tc.first.tc_idx);
                                        copy_v_tc_to_v_tasks_with_pos(v_cores);
                                        priority_assignment(v_cores[i]);
                                        src_core_idx = i;
                                }
                        }
                }
        }

        /* insert tc to target core by core copy */
        for (unsigned int i = 0; i < v_cores.size(); i++) {
                if (v_cores[i].id == dst_b.id) {
                        rplc_core_by_id(v_cores, dst_b);
                        copy_v_tc_to_v_tasks_with_pos(v_cores);
                        priority_assignment(v_cores[i]);
                        dst_core_idx = i;

                        if (v_cores[i].flag == SCHED_FAILED) {
                                printf("ERR! dst Displacement WCRT of Core %d should have succeeded!\n", 
                                                v_cores[i].id);
                                printf("Core %d flag %d\n", 
                                                v_cores[i].id, v_cores[i].flag);
                                exit(0);
                        }
                }
        }
        printf("Removed TC %d idx: %d from Core %d\n", tc.first.id, 
                        tc.first.tc_idx, v_cores[src_core_idx].id);
        printf("Inserted TC %d idx: %d in Core %d\n", tc.first.id, 
                        tc.first.tc_idx, v_cores[dst_core_idx].id);
        printf("<---------------- DISPLACEMENT SUCCESS ---------------->\n\n\n");
}

void displacement(vector<struct core> &v_cores)
{
        int gcd;
        int flag;
        int load;
        int state;
        int is_found;
        struct core dst_b;
        pair<struct tc, int> tc;
        vector<struct core> v_dst_cores;
        vector<pair<struct tc, int>> v_tcs;

        flag = NO;
        state = NO;
        is_found = NO;
        gcd = 0;
        load = 0;
        dst_b = {0};
        tc.first = {0};
        tc.second = 0;

        /* take next unschedulable tc */
        _store_tcs_disp(v_cores, v_tcs, flag);
        for (unsigned int i = 0; i < v_tcs.size(); i++)
                printf("TC %-3d  tc_idx %d from unfeasible Core %-3d\n", 
                                v_tcs[i].first.id, 
                                v_tcs[i].first.tc_idx, 
                                v_tcs[i].second);
        printf("\n");

        if (flag == NO)
                return;

        while (1) {
                state = NO;
                /* find a schedulable core that has enough space for the tc to fit */
                for (unsigned int i = 0; i < v_tcs.size(); i++) {
                        printf("Try to displace TC %-3d tc_idx %d from Core %-3d\n", 
                                        v_tcs[i].first.id, 
                                        v_tcs[i].first.tc_idx, 
                                        v_tcs[i].second);
                        v_dst_cores.clear();
                        for (unsigned int j = 0; j < v_cores.size(); j++) {
                                if (v_cores[j].flag == SCHED_FAILED)
                                        continue;
                                if (v_tcs[i].second == v_cores[j].id)
                                        continue;
                                /* if tc moved too many times skip */
                                if (v_tcs[i].first.disp_count == MAX_DISP_COUNT) 
                                        break;

                                /* color filter */
                                if (v_tcs[i].first.color != v_cores[j].color && 
                                                v_tcs[i].first.color != WHITE)
                                        continue;

                                /* search for dst cores that can accomodate tc */
                                load = check_if_fit_tc(v_cores[j], v_tcs[i].first, gcd);

                                if (load <= v_cores[j].phi) {
                                        /* add tc to potential core */
                                        v_dst_cores.push_back(v_cores[j]);
                                        add_tc_to_core(v_dst_cores.back(), v_tcs[i].first, load, gcd);
                                }
                        }
                        /* test dst cores and save best core */
                        is_found = _search_for_disp(v_dst_cores, v_tcs[i].first.id, i, dst_b);

                        /* if core not found continue */
                        if (is_found == YES) {
                                /* displace */
                                tc.first = {0};
                                tc.second = dst_b.id;
                                tc = v_tcs[i];
                                v_tcs[i].first.disp_count++;
                                v_tcs[i].second = dst_b.id;
                                _disp(v_cores, tc, dst_b);
                                is_found = NO;
                                state = YES;
                        }                
                }
                if (state == NO)
                        break;
        }
}

void swapping(vector<struct core> &v_cores)
{
        int flag;
        int state;
        int src_color;
        int dst_color;
        struct core dst_core;
        struct core src_core;
        vector<pair<struct tc, int>> v_tcs;

        flag = NO;
        state = NO;

        /* store fail_cores */
        _store_tcs_swap(v_cores, v_tcs, flag);
        for (unsigned int i = 0; i < v_tcs.size(); i++)
                printf("TC %-3d from unfeasible Core %-3d\n", 
                                v_tcs[i].first.id, v_tcs[i].second);

        if (flag == NO)
                return;

        while (1) {
                state = NO;
                /* iterate over unsched tcs */
                for (unsigned int i = 0; i < v_tcs.size(); i++) {
                        for (unsigned int j = 0; j < v_tcs.size(); j++) {
                                /* skip tcs in same core */
                                if (v_tcs[i].second == v_tcs[j].second)
                                        continue;

                                /* if tc has been moved too many times skip */
                                if (v_tcs[i].first.swap_count > MAX_SWAP_COUNT || 
                                                v_tcs[j].first.swap_count > MAX_SWAP_COUNT)
                                        continue;

                                /* none white */
                                if (v_tcs[i].first.color != WHITE && v_tcs[j].first.color != WHITE) {
                                        src_color = 0;
                                        dst_color = 0;
                                        src_color = get_color_by_id(v_cores, v_tcs[i].second);
                                        dst_color = get_color_by_id(v_cores, v_tcs[j].second);
                                        if (v_tcs[i].first.color == src_color && v_tcs[j].first.color == dst_color) {
                                                if (src_color == dst_color) {
                                                        printf("Trying to swap src TC %d tc_idx %d from Core %d with dst TC %d idx: %d from Core %d\n",
                                                                        v_tcs[i].first.id, v_tcs[i].first.tc_idx, v_tcs[i].second,
                                                                        v_tcs[j].first.id, v_tcs[j].first.tc_idx, v_tcs[j].second);

                                                        /* search if swap is possible */
                                                        dst_core = {0};
                                                        src_core = {0};
                                                        flag = NO;
                                                        flag = _search_for_swap(v_cores, v_tcs[i], 
                                                                        v_tcs[j], dst_core, src_core);

                                                        if (flag == YES) {
                                                                _swap(v_cores, dst_core, src_core);
                                                                state = YES;
                                                                break;
                                                        }
                                                }

                                        }

                                }

                                /* both white */
                                if (v_tcs[i].first.color == WHITE && v_tcs[j].first.color == WHITE) {
                                        printf("Trying to swap src TC %d tc_idx %d from Core %d with dst TC %d idx: %d from Core %d\n",
                                                        v_tcs[i].first.id, v_tcs[i].first.tc_idx, v_tcs[i].second,
                                                        v_tcs[j].first.id, v_tcs[j].first.tc_idx, v_tcs[j].second);

                                        /* search if swap is possible */
                                        dst_core = {0};
                                        src_core = {0};
                                        flag = NO;
                                        flag = _search_for_swap(v_cores, v_tcs[i], 
                                                        v_tcs[j], dst_core, src_core);

                                        if (flag == YES) {
                                                _swap(v_cores, dst_core, src_core);
                                                state = YES;
                                                break;
                                        }
                                }

                                /* src white dst not white */
                                if (v_tcs[i].first.color == WHITE && v_tcs[j].first.color != WHITE) {
                                        src_color = 0;
                                        dst_color = 0;
                                        src_color = get_color_by_id(v_cores, v_tcs[i].second);
                                        dst_color = get_color_by_id(v_cores, v_tcs[j].second);
                                        if (src_color == dst_color) {
                                                printf("Trying to swap src TC %d tc_idx %d from Core %d with dst TC %d idx: %d from Core %d\n",
                                                                v_tcs[i].first.id, v_tcs[i].first.tc_idx, v_tcs[i].second,
                                                                v_tcs[j].first.id, v_tcs[j].first.tc_idx, v_tcs[j].second);

                                                /* search if swap is possible */
                                                dst_core = {0};
                                                src_core = {0};
                                                flag = NO;
                                                flag = _search_for_swap(v_cores, v_tcs[i], 
                                                                v_tcs[j], dst_core, src_core);

                                                if (flag == YES) {
                                                        _swap(v_cores, dst_core, src_core);
                                                        state = YES;
                                                        break;
                                                }
                                        }
                                }

                                /* src not white dst white */
                                if (v_tcs[i].first.color != WHITE && v_tcs[j].first.color == WHITE) {
                                        src_color = 0;
                                        dst_color = 0;
                                        src_color = get_color_by_id(v_cores, v_tcs[i].second);
                                        dst_color = get_color_by_id(v_cores, v_tcs[j].second);
                                        if (src_color == dst_color) {
                                                printf("Trying to swap src TC %d tc_idx %d from Core %d with dst TC %d idx: %d from Core %d\n",
                                                                v_tcs[i].first.id, v_tcs[i].first.tc_idx, v_tcs[i].second,
                                                                v_tcs[j].first.id, v_tcs[j].first.tc_idx, v_tcs[j].second);

                                                /* search if swap is possible */
                                                dst_core = {0};
                                                src_core = {0};
                                                flag = NO;
                                                flag = _search_for_swap(v_cores, v_tcs[i], 
                                                                v_tcs[j], dst_core, src_core);

                                                if (flag == YES) {
                                                        _swap(v_cores, dst_core, src_core);
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
