#include "print.h"
#include "sched_analysis.h"

static void _find_hp_tasks(vector<struct task> &v_tasks, 
                vector<struct task> &hp_tasks, struct task &tau, int &r_prev)
{
        for (unsigned int i = 0; i < v_tasks.size(); i++) {
                if (v_tasks[i].p < tau.p)
                        hp_tasks.push_back(v_tasks[i]);
        }
}

static void _resp_part(vector<struct task> &v_tasks, int &flag)
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

                v_tasks[i].r = v_tasks[i].c + r_prev + (WCBT * BFCT);
                r_prev = v_tasks[i].r;

                if (r_prev > v_tasks[i].t)
                        flag = SCHED_FAILED;
        }
}

int wcrt_part(vector<struct task> &v_tasks)
{
        int flag;

        flag = -1;

        _resp_part(v_tasks, flag);

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

int wcrt(vector<struct task> &v_tasks)
{
        int flag;

        flag = -1;

        _resp(v_tasks, flag);

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

static void _save_priorities(struct core &b)
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

static void _pswap(struct core &b, int p, int tc_idx, int tc_id, int uniq_id)
{
        /* starting new p */
        int newp;
        vector<int> v_p;

        newp = p + 1;

        /* store hp */
        for (unsigned int i = 0; i < b.v_tasks.size(); i++) {
                if (b.v_tasks[i].is_let == YES)
                        continue;
                if (b.v_tasks[i].idx.tc_idx == tc_idx)
                        continue;
                if (b.v_tasks[i].uniq_id == uniq_id)
                        continue;
                if (b.v_tasks[i].p < p && b.v_tasks[i].tc_id == tc_id)
                        continue;

                if (b.v_tasks[i].p < p)
                        v_p.push_back(b.v_tasks[i].p);
        }
        /* sort in decreasing order of priority to start with lowest hp */
        sort_dec_int(v_p);

        /* pswap */
        for (unsigned int z = 0; z < v_p.size(); z++) {
                for (unsigned int i = 0; i < b.v_tasks.size(); i++) {
                        if (b.v_tasks[i].is_let == YES)
                                continue;
                        if (b.v_tasks[i].idx.tc_idx == tc_idx)
                                continue;

                        if (b.v_tasks[i].p == v_p[z]) {
                                for (unsigned int j = i; j < b.v_tasks.size(); j++) {
                                        if (b.v_tasks[j].p == p)
                                                continue;
                                        if (b.v_tasks[j].idx.tc_idx == tc_idx && b.v_tasks[j].p < p)
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
                        printf("Core %d SCHED_OK with priority swapping\n", b.id);
                        return;

                } else {
                        b.flag = SCHED_FAILED;
                        return;
                }
        }
}

static void _base_assignment(struct core &b)
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

static void _pswapping(struct core &b)
{
        struct core tmp_b;

        sort_inc_task_priority(b.v_tasks);

        /* iterate in descending order */
        for (unsigned int i = b.v_tasks.size() - 1; i > 0; i--) {
                if (b.v_tasks[i].p == 0) {
                        printf("\nERR! Core %d tau %d p %d idx %d\n", 
                                        b.id, b.v_tasks[i].id, b.v_tasks[i].p, 
                                        b.v_tasks[i].idx.tc_idx);
                        exit(0);
                }
                if (b.v_tasks[i].r > b.v_tasks[i].t) {
                        tmp_b = {0};
                        tmp_b = b;

                        /* priority swapping */
                        _pswap(tmp_b, b.v_tasks[i].p, 
                                        tmp_b.v_tasks[i].idx.tc_idx, 
                                        tmp_b.v_tasks[i].tc_id, 
                                        b.v_tasks[i].uniq_id);
                        if (tmp_b.flag == SCHED_OK) {
                                b = tmp_b;
                        }
                }
        }
}

void priority_assignment(struct core &b)
{
        _base_assignment(b);
        if (b.flag == SCHED_FAILED) {
                printf("Core %d SCHED_FAILED\n", b.id);
                _pswapping(b);
        } else
                printf("Core %d SCHED_OK\n", b.id);
}

void sched_analysis(vector<struct core> &v_cores, struct context &ctx)
{
        sort_inc_core_load_rem(v_cores);
        copy_v_tc_to_v_tasks_with_pos(v_cores);

        for (unsigned int i = 0; i < v_cores.size(); i++) {
                if (v_cores[i].load == 0)
                        continue;
                else
                        priority_assignment(v_cores[i]);
        }

        ctx.p.sched_rate_allo = sched_rate(v_cores, ctx);
        ctx.p.sched_imp_allo = ctx.sched_ok_count;
}
