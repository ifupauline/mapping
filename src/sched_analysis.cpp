#include "sched_analysis.h"

static void _assign_priority(vector<struct bin> &v_bins)
{
        for (unsigned int i = 0; i < v_bins.size(); i++) {
                v_bins[i].v_tasks.clear();
                for (unsigned int j = 0; j < v_bins[i].v_itms.size(); j++) {
                        copy_tc_to_v_tasks(v_bins[i], i, j);
                }
        }

        /* assign and copy back unique priorities to each tasks */
        for (unsigned int i = 0; i < v_bins.size(); i++) {
                assign_unique_priorities(v_bins[i]);
                copy_back_prio_to_tc(v_bins[i]);
        }
}

static void _find_hp_tasks(vector<struct task> &v_tasks, vector<struct task> &hp_tasks,  
                struct task &tau, int &r_prev)
{
        for (unsigned int i = 0; i < v_tasks.size(); i++) {
                if (v_tasks[i].p < tau.p)
                        hp_tasks.push_back(v_tasks[i]);
        }
}

static void _fixpoint(vector<struct task> &hp_tasks, struct task &tau, 
                int &r_curr, int &r_prev, int &ret)
{
        int tmp;

        tmp = tau.c;

        if (tau.c > tau.t) {
                printf("ERROR Execution Time of tau %d > Period\n\n", tau.id);
                exit(0);
        }

        for (unsigned int i = 0; i < hp_tasks.size(); i++)
                tmp += (ceilf((float)r_prev/(float)hp_tasks[i].t) * hp_tasks[i].c);

        r_curr = tmp;

        if (r_curr > tau.t) {
                //printf("Current Response Time of tau %d: %d > period %d exit loop\n\n", 
                //                tau.id, r_curr, tau.t);
                tau.r = -1;
                ret = SCHED_FAILED;
                return;
        }

        if (r_curr > r_prev) {
                r_prev = r_curr;
                _fixpoint(hp_tasks, tau, r_curr, r_prev, ret);
        }

        if (r_curr == r_prev) {
                //printf("WCRT of tau %d: %d\n", tau.id, r_curr);
                tau.r = r_curr;
                ret = SCHED_OK;
                if (tau.r > tau.t) {
                        printf("ERR! tau.r: %d > tau.t: %d\n", tau.r, tau.t);
                        exit(0);
                }
                return;
        }
}

void wcrt_bin(struct bin &b, int bin_idx)
{
        int ret;

        b.v_tasks.clear();
        for (unsigned int y = 0; y < b.v_itms.size(); y++)
                copy_tc_to_v_tasks(b, bin_idx, y);

        /* assign unique priorities to each tasks */
        assign_unique_priorities(b);

        /* test wcrt */
        ret = wcrt(b.v_tasks);
        if (ret == SCHED_OK) 
                b.flag = SCHED_OK;
        
        else if (ret == SCHED_FAILED)
                b.flag = SCHED_FAILED;
                
        copy_back_prio_to_tc(b);
        copy_back_resp_to_tc(b);
        compute_bin_load(b);
}

int wcrt(vector<struct task> &v_tasks)
{
        int ret;
        int flag;
        int r_curr;
        int r_prev;
        vector<struct task> hp_tasks;

        ret = -1;
        flag = SCHED_OK;

        /* sort decreasing priority order 1 -> n */
        sort_inc_task_priority(v_tasks);

        /* compute WCRT for each task */
        for (unsigned int i = 0; i < v_tasks.size(); i++) {
                r_curr = 0;
                r_prev = 0;
                hp_tasks.clear();
                _find_hp_tasks(v_tasks, hp_tasks, v_tasks[i], r_prev);
                /* recursive */
                _fixpoint(hp_tasks, v_tasks[i], r_curr, r_prev, ret);

                if (ret == SCHED_FAILED)
                        flag = SCHED_FAILED;
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

void wcrt_v_bins(vector<struct bin> &v_bins, struct context &ctx)
{
        _assign_priority(v_bins);

        for (unsigned int i = 0; i < v_bins.size(); i++) {
                for (unsigned int j = 0; j < v_bins[i].v_itms.size(); j++)
                        v_bins[i].flag = wcrt(v_bins[i].v_tasks);

                if (v_bins[i].flag == SCHED_FAILED) {
                        printf("Core %d SCHED_FAILED\n", v_bins[i].id);
                } else {
                        printf("Core %d SCHED_OK\n", v_bins[i].id);
                }
        }

        /* copy back new response time to original tasks in tc */
        for (unsigned int i = 0; i < v_bins.size(); i++)
                copy_back_resp_to_tc(v_bins[i]);
}

float sched_rate(vector<struct bin> &v_bins, struct context &ctx)
{
        int tc_count;
        float sched_rate;

        tc_count = 0;
        ctx.sched_ok_count = 0;
        ctx.sched_failed_count = 0;

        for (unsigned int i = 0; i < v_bins.size(); i++) {
                if (v_bins[i].flag == SCHED_OK)
                        ctx.sched_ok_count++;
        }

        for (unsigned int i = 0; i < v_bins.size(); i++) {
                if (v_bins[i].flag == SCHED_FAILED)
                        ctx.sched_failed_count++;
        }

        ctx.sched_tcok_count = 0;
        ctx.sched_tcfailed_count = 0;

        for (unsigned int i = 0; i < v_bins.size(); i++) {
                for (unsigned int j = 0; j < v_bins[i].v_itms.size(); j++) {
                        tc_count++;
                }
        }

        for (unsigned int i = 0; i < v_bins.size(); i++) {
                for (unsigned int j = 0; j < v_bins[i].v_itms.size(); j++) {
                        for (unsigned int k = 0; k < v_bins[i].v_itms[j].tc.v_tasks.size(); k++) {
                                if (v_bins[i].v_itms[j].tc.v_tasks[k].r == -1) {
                                        ctx.sched_tcfailed_count++;
                                        continue;
                                }
                        }
                }
        }
        ctx.sched_tcok_count = tc_count - ctx.sched_tcfailed_count;
        sched_rate = (float)ctx.sched_ok_count / (float)ctx.bins_count;

        return sched_rate;
}

void assign_unique_priorities(struct bin &b)
{
        for (unsigned int i = 0; i < b.v_tasks.size(); i++)
                b.v_tasks[i].p = i + 1;
}

void assign_new_priorities(struct bin &b, int &p, int &itm_idx)
{
        /* starting new p */
        p = p + 1;
        printf("Updating Priorities... with p: %d\n", p);

        /* assign new priorities */
        for (unsigned int i = 0; i < b.v_tasks.size(); i++) {
                if (b.v_tasks[i].idx.itm_idx == (int)itm_idx)
                        continue;

                if (b.v_tasks[i].p < p) {
                        for (unsigned int j = 0; j < b.v_itms[itm_idx].tc.v_tasks.size(); j++) {
                                if (b.v_itms[itm_idx].tc.v_tasks[j].p == p) {
                                        printf("priority %d already assigned\n", p);
                                        p++;
                                }
                        }
                        b.v_tasks[i].p = p;
                        p = p + 1;
                        printf("tau %d p: %d\n", b.v_tasks[i].id, 
                                        b.v_tasks[i].p);
                }
        }
}
