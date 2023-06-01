#include "print.h"
#include "sched_analysis.h"

int bfdu_wcrt_count = 0;
int wfdu_wcrt_count = 0;
int frst_wcrt_count = 0;
int bfdu_syst_state = NO;
int wfdu_syst_state = NO;
int frst_syst_state = NO;
float bfdu_sched_time = 0;
float wfdu_sched_time = 0;
float frst_sched_time = 0;

static void _find_hp_tasks(vector<struct task> &v_tasks, vector<struct task> &hp_tasks,  
                struct task &tau, int &r_prev)
{
        for (unsigned int i = 0; i < v_tasks.size(); i++) {
                if (v_tasks[i].p < tau.p)
                        hp_tasks.push_back(v_tasks[i]);
        }
}

static void _resp(vector<struct task> &v_tasks, int &flag)
{
        int r_prev;
        vector<struct task> hp_tasks;

        r_prev = 0;
        flag = SCHED_OK;

        sort_inc_task_priority(v_tasks);

        for (unsigned int i = 0; i < v_tasks.size(); i++) {
                hp_tasks.clear();
                _find_hp_tasks(v_tasks, hp_tasks, v_tasks[i], r_prev);

                if (v_tasks[i].c > v_tasks[i].t) {
                        printf("ERR! Execution Time %d of tau %d > Period %d\n\n", 
                                        v_tasks[i].c, v_tasks[i].id, v_tasks[i].t);
                        exit(0);
                }

                if (v_tasks[i].p == 1) {
                        v_tasks[i].r = v_tasks[i].c;
                        r_prev = v_tasks[i].r;
                        continue;
                }

                v_tasks[i].r = v_tasks[i].c + r_prev;
                r_prev = v_tasks[i].r;

                if (r_prev > v_tasks[i].t)
                        flag = SCHED_FAILED;
        }
}

int wcrt(vector<struct task> &v_tasks)
{
        int flag;
        clock_t start, end;

        flag = -1;

        if (bfdu_syst_state == YES)
                start = clock();
        if (wfdu_syst_state == YES)
                start = clock();
        if (frst_syst_state == YES)
                start = clock();

        _resp(v_tasks, flag);

        if (bfdu_syst_state == YES) {
                end = clock();
                bfdu_sched_time += ((float) (end - start)) / CLOCKS_PER_SEC;
                bfdu_wcrt_count++;
        }

        if (wfdu_syst_state == YES) {
                end = clock();
                wfdu_sched_time += ((float) (end - start)) / CLOCKS_PER_SEC;
                wfdu_wcrt_count++;
        }

        if (frst_syst_state == YES) {
                end = clock();
                frst_sched_time += ((float) (end - start)) / CLOCKS_PER_SEC;
                frst_wcrt_count++;
        }

        if (flag == SCHED_OK)
                return SCHED_OK;

        else if (flag == SCHED_FAILED)
                return SCHED_FAILED;
        else {
                printf("ERR! wcrt!\n");
                exit(0);
        }
        return -1;
}

static void _save_priorities(struct bin &b)
{
        int p;

        p = 2;

        sort_inc_task_priority(b.v_tasks);

        for (unsigned int i = 0; i < b.v_tasks.size(); i++) {
                if (b.v_tasks[i].is_let == YES) {
                        b.v_tasks[i].p = 1;
                        continue;
                }
                b.v_tasks[i].p = p;
                p++;
        }
}

static void _reassign(struct bin &b, int p, int itm_idx, int tc_id, int uniq_id)
{
        /* starting new p */
        int newp;
        vector<int> v_p;

        newp = p + 1;

        /* store hp */
        for (unsigned int i = 0; i < b.v_tasks.size(); i++) {
                if (b.v_tasks[i].is_let == YES)
                        continue;
                if (b.v_tasks[i].idx.itm_idx == itm_idx)
                        continue;
                if (b.v_tasks[i].uniq_id == uniq_id)
                        continue;
                if (b.v_tasks[i].p < p && b.v_tasks[i].tc_id == tc_id) {
                        continue;
                }

                if (b.v_tasks[i].p < p)
                        v_p.push_back(b.v_tasks[i].p);
        }
        /* sort in decreasing order of priority to start with lowest hp */
        sort_dec_int(v_p);

        /* reassign */
        for (unsigned int z = 0; z < v_p.size(); z++) {
                for (unsigned int i = 0; i < b.v_tasks.size(); i++) {
                        if (b.v_tasks[i].is_let == YES)
                                continue;
                        if (b.v_tasks[i].idx.itm_idx == itm_idx)
                                continue;

                        if (b.v_tasks[i].p == v_p[z]) {
                                for (unsigned int j = i; j < b.v_tasks.size(); j++) {
                                        if (b.v_tasks[j].p == p)
                                                continue;
                                        if (b.v_tasks[j].idx.itm_idx == itm_idx && b.v_tasks[j].p < p)
                                                continue;
                                        b.v_tasks[j].p = newp;
                                        newp++;
                                }
                        }
                }
                _save_priorities(b);

                b.flag = wcrt(b.v_tasks);
                if (b.flag == SCHED_OK) {
                        copy_back_prio_to_tc(b);
                        copy_back_resp_to_tc(b);
                        printf("Core %d SCHED_OK with reassignment\n", b.id);
                        return;

                } else {
                        b.flag = SCHED_FAILED;
                        return;
                }
        }
}

static void _base_assignment(struct bin &b)
{
        int p;

        p = 2;

        /* no need to check for TC belonging to the same TC coze of ordering */
        sort_inc_task_id(b.v_tasks);

        for (unsigned int i = 0; i < b.v_tasks.size(); i++) {
                if (b.v_tasks[i].is_let == YES) {
                        b.v_tasks[i].p = 1;
                        continue;
                }
                b.v_tasks[i].p = p;
                p++;
        }
        b.flag = wcrt(b.v_tasks);
        copy_back_prio_to_tc(b);
        copy_back_resp_to_tc(b);
}

static void _reassignment(struct bin &b)
{
        //int ret;
        struct bin tmp_b;

        //ret = NO;

        //ret = is_frag_same_tc(b);

        sort_inc_task_priority(b.v_tasks);

        /* iterate in descending order */
        for (unsigned int i = b.v_tasks.size() - 1; i > 0; i--) {
                if (b.v_tasks[i].p == 0) {
                        printf("\nERR! Core %d tau %d p %d idx %d\n", 
                                        b.id, b.v_tasks[i].id, b.v_tasks[i].p, 
                                        b.v_tasks[i].idx.itm_idx);
                        exit(0);
                }
                if (b.v_tasks[i].r > b.v_tasks[i].t) {
                        tmp_b = {0};
                        tmp_b = b;
                        /* priority swapping */
                        _reassign(tmp_b, b.v_tasks[i].p, 
                                        tmp_b.v_tasks[i].idx.itm_idx, 
                                        tmp_b.v_tasks[i].tc_id, 
                                        b.v_tasks[i].uniq_id);
                        if (tmp_b.flag == SCHED_OK) {
                                b = tmp_b;
                                //if (ret == YES) {
                                //        print_core(b);
                                //        exit(0);
                                //}
                        }
                }
        }
}

void priority_assignment(struct bin &b)
{
        _base_assignment(b);
        if (b.flag == SCHED_FAILED) {
                printf("Core %d SCHED_FAILED\n", b.id);
                _reassignment(b);
        } else
                printf("Core %d SCHED_OK\n", b.id);
}

void sched_analysis(vector<struct bin> &v_bins, struct context &ctx)
{
        if (ctx.prm.a == BFDU_F)
                bfdu_syst_state = YES;
        if (ctx.prm.a == WFDU_F)
                wfdu_syst_state = YES;
        if (ctx.prm.a == FRST_F)
                frst_syst_state = YES;
        sort_inc_bin_load_rem(v_bins);
        copy_v_tc_to_v_tasks_with_pos(v_bins);

        for (unsigned int i = 0; i < v_bins.size(); i++)
                priority_assignment(v_bins[i]);

        ctx.p.sched_rate_allo = sched_rate(v_bins, ctx);
        ctx.p.sched_imp_allo = ctx.sched_ok_count;
}

void verify_tc_schedulability(vector<vector<struct item>> &v_frag_itms, 
                vector<struct item> &v_non_frag_itms, struct context &ctx)
{
        printf("+=================================+\n");
        printf("| TC SCHEDULABILITY               |\n");
        printf("+=================================+\n");

        int flag;

        for (unsigned int i = 0; i < v_non_frag_itms.size(); i++) {
                flag = NO;
                for (unsigned int j = 0; j < v_non_frag_itms[i].v_tasks.size(); j++) {
                        if (v_non_frag_itms[i].v_tasks[j].r > v_non_frag_itms[i].v_tasks[j].t) {
                                printf("TC %d failed --> r %-6d > t %-6d\n", v_non_frag_itms[i].id, 
                                                v_non_frag_itms[i].v_tasks[j].r, 
                                                v_non_frag_itms[i].v_tasks[j].t);
                                flag = YES;
                        }
                }
                if (flag == NO)
                        ctx.e2e_ok_count++;
                else
                        ctx.e2e_failed_count++;
        }
        printf("----------------------------------------\n");

        for (unsigned int i = 0; i < v_frag_itms.size(); i++) {
                flag = NO;
                for (unsigned int j = 0; j < v_frag_itms[i].size(); j++) {
                        /* check wcrt */
                        for (unsigned int k = 0; k < v_frag_itms[i][j].v_tasks.size(); k++) {
                                if (v_frag_itms[i][j].v_tasks[k].r > v_frag_itms[i][j].v_tasks[k].t) {
                                        printf("TC %-3d tc_idx %-3d failed --> r %-6d > t %-6d\n", 
                                                        v_frag_itms[i][j].id, v_frag_itms[i][j].tc_idx,
                                                        v_frag_itms[i][j].v_tasks[k].r, 
                                                        v_frag_itms[i][j].v_tasks[k].t);
                                        flag = YES;
                                }
                        }
                }
                if (flag == NO)
                        ctx.e2e_ok_count++;
                else
                        ctx.e2e_failed_count++;
        }
        printf("Number of TC schedulable:   %d\n", ctx.e2e_ok_count);
        printf("Number of TC unschedulable: %d\n", ctx.e2e_failed_count);
}
