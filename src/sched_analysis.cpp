#include "sched_analysis.h"

static int cmp_inc_priority(const struct task &a, const struct task &b)
{
        return a.p < b.p;
}

static void sort_inc_priority(vector<struct task> &v_tasks)
{
        sort(v_tasks.begin(), v_tasks.end(), cmp_inc_priority);
}

static int cmp_inc_id(const struct task &a, const struct task &b)
{
        return a.id < b.id;
}

static void sort_inc_id(vector<struct task> &v_tasks)
{
        sort(v_tasks.begin(), v_tasks.end(), cmp_inc_id);
}

void priority_assignment(vector<struct bin> &v_bins)
{
        int bin_idx;
        int itm_idx;
        int task_idx;

        for (unsigned int i = 0; i < v_bins.size(); i++) {
                v_bins[i].v_tasks.clear();
                for (unsigned int j = 0; j < v_bins[i].v_itms.size(); j++) {
                        sort_inc_id(v_bins[i].v_itms[j].tc.v_tasks);
                        for (unsigned int k = 0; k < v_bins[i].v_itms[j].tc.v_tasks.size(); k++) {
                                v_bins[i].v_itms[j].tc.v_tasks[k].id = k;
                                v_bins[i].v_tasks.push_back(v_bins[i].v_itms[j].tc.v_tasks[k]);
                                v_bins[i].v_tasks.back().idx.bin_idx = i;
                                v_bins[i].v_tasks.back().idx.itm_idx = j;
                                v_bins[i].v_tasks.back().idx.task_idx = k;
                        }
                }
                sort_inc_id(v_bins[i].v_tasks);
        }

        /* assign unique priorities to each tasks */
        for (unsigned int i = 0; i < v_bins.size(); i++) {
                for (unsigned int j = 0; j < v_bins[i].v_tasks.size(); j++)
                        v_bins[i].v_tasks[j].p = j + 1;
        }

        /* copy back new priority to original tasks in tc */
        for (unsigned int i = 0; i < v_bins.size(); i++) {
                for (unsigned int j = 0; j < v_bins[i].v_tasks.size(); j++) {
                        bin_idx = v_bins[i].v_tasks[j].idx.bin_idx;
                        itm_idx = v_bins[i].v_tasks[j].idx.itm_idx;
                        task_idx = v_bins[i].v_tasks[j].idx.task_idx;
                        v_bins[bin_idx].v_itms[itm_idx].tc.v_tasks[task_idx].p = v_bins[i].v_tasks[j].p;
                }
        }
}

void priority_reassignment(struct bin &b, unsigned int &itm_id, int &p)
{
        int flag;
        int itm_idx;
        int task_idx;
        struct bin b_tmp;

        b_tmp = b;
        flag = -1;

        /* starting new p */
        p = p + 1;
        printf("Updating Priorities... with p: %d\n", p);

        /* assign new priorities */
        for (unsigned int i = 0; i < b_tmp.v_tasks.size(); i++) {
                if (b_tmp.v_tasks[i].idx.itm_idx == (int)itm_id)
                        continue;

                if (b_tmp.v_tasks[i].p < p) {
                        for (unsigned int j = 0; j < b_tmp.v_itms[itm_id].tc.v_tasks.size(); j++) {
                                if (b_tmp.v_itms[itm_id].tc.v_tasks[j].p == p) {
                                        printf("priority %d already assigned\n", p);
                                        p++;
                                }
                        }
                        b_tmp.v_tasks[i].p = p;
                        p = p + 1;
                        printf("tau %d p: %d\n", b_tmp.v_tasks[i].id, b_tmp.v_tasks[i].p);
                }
        }

        flag = wcrt(b_tmp.v_tasks);

        /* copy back response time  and priorities to tc */
        for (unsigned int j = 0; j < b_tmp.v_tasks.size(); j++) {
                itm_idx = b_tmp.v_tasks[j].idx.itm_idx;
                task_idx = b_tmp.v_tasks[j].idx.task_idx;
                b_tmp.v_itms[itm_idx].tc.v_tasks[task_idx].r = b_tmp.v_tasks[j].r;
                b_tmp.v_itms[itm_idx].tc.v_tasks[task_idx].p = b_tmp.v_tasks[j].p;
        }

        if (flag == SCHED_OK) {
                b = b_tmp;
                b.flag = SCHED_OK;
                printf("Core %d SCHED_OK with new priority assignment\n\n", b_tmp.id);
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
                b.flag = SCHED_FAILED;
                printf("Core %d SCHED_FAILED with new priority assignment\n\n", b_tmp.id);
                return;

        } else {
                printf("ERR! priority reassignment\n");
                exit(0);
        }
}

static void find_hp_tasks(vector<struct task> &v_tasks, vector<struct task> &hp_tasks,  
                struct task &tau, int &r_prev)
{
        for (unsigned int i = 0; i < v_tasks.size(); i++) {
                if (v_tasks[i].p < tau.p)
                        hp_tasks.push_back(v_tasks[i]);
        }
}

static void fixpoint(vector<struct task> &hp_tasks, struct task &tau, 
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

        /* YOU MUST TEST THIS CONDITION FIRST */
        if (r_curr > tau.t) {
                //printf("Current Response Time of tau %d: %d > period %d exit loop\n\n", 
                //                tau.id, r_curr, tau.t);
                tau.r = -1;
                ret = SCHED_FAILED;
                return;
        }

        if (r_curr > r_prev) {
                r_prev = r_curr;
                fixpoint(hp_tasks, tau, r_curr, r_prev, ret);
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
        sort_inc_priority(v_tasks);

        /* compute WCRT for each task */
        for (unsigned int i = 0; i < v_tasks.size(); i++) {
                r_curr = 0;
                r_prev = 0;
                hp_tasks.clear();
                find_hp_tasks(v_tasks, hp_tasks, v_tasks[i], r_prev);
                /* recursive */
                fixpoint(hp_tasks, v_tasks[i], r_curr, r_prev, ret);

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

void sched_analysis(vector<struct bin> &v_bins, struct context &ctx)
{
        int bin_idx;
        int itm_idx;
        int task_idx;

        for (unsigned int i = 0; i < v_bins.size(); i++) {
                for (unsigned int j = 0; j < v_bins[i].v_itms.size(); j++)
                        v_bins[i].flag = wcrt(v_bins[i].v_tasks);

                if (v_bins[i].flag == SCHED_FAILED) {
                        ctx.sched_failed_count++;
                        printf("Core %d SCHED_FAILED\n", v_bins[i].id);
                } else {
                        ctx.sched_ok_count++;
                        printf("Core %d SCHED_OK\n", v_bins[i].id);
                }
        }

        /* copy back new response time to original tasks in tc */
        for (unsigned int i = 0; i < v_bins.size(); i++) {
                for (unsigned int j = 0; j < v_bins[i].v_tasks.size(); j++) {
                        bin_idx = v_bins[i].v_tasks[j].idx.bin_idx;
                        itm_idx = v_bins[i].v_tasks[j].idx.itm_idx;
                        task_idx = v_bins[i].v_tasks[j].idx.task_idx;
                        v_bins[bin_idx].v_itms[itm_idx].tc.v_tasks[task_idx].r = v_bins[i].v_tasks[j].r;
                }
        }
        ctx.p.sched_rate_bef = (float)ctx.sched_ok_count / (float)ctx.bins_count;
        ctx.p.sched_imp = ctx.p.sched_imp - ctx.sched_ok_count;
}
