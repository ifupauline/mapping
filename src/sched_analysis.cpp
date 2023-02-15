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
                if (v_tasks[i].p <= tau.p) {
                        r_curr += v_tasks[i].c;
                }
        }
        r_prev = r_curr;

        for (unsigned int i = 0; i < v_tasks.size(); i++) {
                if (v_tasks[i].id == tau.id)
                        continue;

                if (v_tasks[i].p <= tau.p)
                        hp_tasks.push_back(v_tasks[i]);
        }
}

static void fixpoint(vector<struct task> &hp_tasks, struct task &tau, 
                int &r_curr, int &r_prev, int &flag)
{
        int tmp;

        tmp = tau.c;

        if (tau.c > tau.t) {
                printf("ERROR Execution Time of tau %d > Period\n\n", tau.id);
                exit(0);
        }

        for (unsigned int i = 0; i < hp_tasks.size(); i++)
                tmp += (ceil((double)r_prev/(double)hp_tasks[i].t) * hp_tasks[i].c);

        r_curr = tmp;

        if (r_curr == r_prev) {
                printf("WCRT of tau %d: %d\n", tau.id, r_curr);
                tau.r = r_curr;
                return;
        }

        if (r_curr > tau.t) {
                printf("Current Response Time of tau %d: %d > period %d exit loop\n\n", 
                                tau.id, r_curr, tau.t);
                flag = SCHED_FAILED;
                return;
        }

        r_prev = r_curr;
        fixpoint(hp_tasks, tau, r_curr, r_prev, flag);
}

int wcrt(vector<struct task> &v_tasks)
{
        int flag;
        int r_curr;
        int r_prev;
        vector<struct task> hp_tasks;

        flag = SCHED_OK;

        /* sort decreasing priority order 1 -> n */
        sort_inc(v_tasks);

        /* compute WCRT for each task */
        for (unsigned int i = 0; i < v_tasks.size(); i++) {
                r_curr = 0;
                r_prev = 0;
                hp_tasks.clear();
                find_hp_tasks(v_tasks, hp_tasks, v_tasks[i], r_curr, r_prev);
                /* recursive */
                fixpoint(hp_tasks, v_tasks[i], r_curr, r_prev, flag);
        }
        return flag;
}
