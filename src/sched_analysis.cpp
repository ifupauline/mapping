#include "sched_analysis.h"

static int cmp_inc(const struct task &a, const struct task &b)
{
        return a.p < b.p;
}

static void sort_inc(vector<struct task> &v_tasks)
{
        sort(v_tasks.begin(), v_tasks.end(), cmp_inc);
}

static void find_hp_tasks(vector<struct task> &v_tasks, vector<struct task> &hp_tasks,  
                struct task &tau, int &r_curr, int &r_prev)
{
        for (unsigned int i = 0; i < v_tasks.size(); i++) {
                if (v_tasks[i].p < tau.p) {
                        r_curr += v_tasks[i].c;
                }
        }
        r_prev = r_curr;

        for (unsigned int i = 0; i < v_tasks.size(); i++) {
                if (v_tasks[i].id == tau.id)
                        continue;

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

        if (r_curr == r_prev) {
                //printf("WCRT of tau %d: %d\n", tau.id, r_curr);
                tau.r = r_curr;
                ret = SCHED_OK;
                return;
        }

        if (r_curr > tau.t) {
                //printf("Current Response Time of tau %d: %d > period %d exit loop\n\n", 
                //                tau.id, r_curr, tau.t);
                tau.r = r_curr;
                ret = SCHED_FAILED;
                return;
        }

        if (r_prev < r_curr) {
                r_prev = r_curr;
                fixpoint(hp_tasks, tau, r_curr, r_prev, ret);
        }
}

int wcrt(vector<struct task> &v_tasks)
{
        int ret;
        int r_curr;
        int r_prev;
        vector<struct task> hp_tasks;

        /* sort decreasing priority order 1 -> n */
        sort_inc(v_tasks);

        /* compute WCRT for each task */
        for (unsigned int i = 0; i < v_tasks.size(); i++) {
                r_curr = 0;
                r_prev = 0;
                hp_tasks.clear();
                find_hp_tasks(v_tasks, hp_tasks, v_tasks[i], r_curr, r_prev);
                /* recursive */
                fixpoint(hp_tasks, v_tasks[i], r_curr, r_prev, ret);

                if (ret == SCHED_FAILED)
                        return SCHED_FAILED;
        }
        return SCHED_OK;
}

int sched_analysis(vector<struct bin> &v_bins, struct context &ctx)
{
        clock_t start, end;

        start = clock();
        for (unsigned int i = 0; i < v_bins.size(); i++) {
                for (unsigned int j = 0; j < v_bins[i].v_itms.size(); j++) {
                        for (unsigned int k = 0; k < v_bins[i].v_itms[j].tc.v_tasks.size(); k++) {
                                v_bins[i].v_tasks.push_back(v_bins[i].v_itms[j].tc.v_tasks[k]);
                                /* save location of task in tc */
                                v_bins[i].v_tasks.back().idx.bin_idx = i;
                                v_bins[i].v_tasks.back().idx.itm_idx = j;
                                v_bins[i].v_tasks.back().idx.task_idx = k;
                        }
                        v_bins[i].flag = wcrt(v_bins[i].v_tasks);

                        if (v_bins[i].flag == SCHED_OK) {
                                printf("Bin %d SCHED_OK\n", v_bins[i].id);
                        }

                        if (v_bins[i].flag == SCHED_FAILED) {
                                printf("Bin %d SCHED_FAILED\n", v_bins[i].id);
                        }
                }
        }
        end = clock();
        ctx.sched_time = ((float) (end - start)) / CLOCKS_PER_SEC;

        /* copy back new response time to original tasks in tc */
        for (unsigned int i = 0; i < v_bins.size(); i++) {
                for (unsigned int j = 0; j < v_bins[i].v_tasks.size(); j++) {
                        int bin_idx = v_bins[i].v_tasks[j].idx.bin_idx;
                        int itm_idx = v_bins[i].v_tasks[j].idx.itm_idx;
                        int task_idx = v_bins[i].v_tasks[j].idx.task_idx;
                        v_bins[bin_idx].v_itms[itm_idx].tc.v_tasks[task_idx].r = v_bins[i].v_tasks[j].r;
                }
        }
        return 0;
}
