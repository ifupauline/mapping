#include "stdio.h"
#include "math.h"

#include <vector>
#include <algorithm>

using namespace std;

struct task {
        int c;
        int t;
        int d;
        int p;
        int r;
        int id;
};

static void print_v_tasks(vector<struct task> &v_tasks)
{
        for (unsigned int i = 0; i < v_tasks.size(); i++) {
                printf("task.id: %d priority: %d\n", v_tasks[i].id, v_tasks[i].p);
        }
}

static int cmp_inc(const struct task &a, const struct task &b)
{
        return a.p < b.p;
}

static void sort_inc(vector<struct task> &v_tasks)
{
        sort(v_tasks.begin(), v_tasks.end(), cmp_inc);
}

static void init(vector<struct task> &v_tasks, struct task &tau, 
                int &r_curr, int &r_prev)
{
        for (unsigned int i = 0; i < v_tasks.size(); i++) {
                if (v_tasks[i].p <= tau.p) {
                        r_curr += v_tasks[i].c;
                }
        }
        r_prev = r_curr;
}

static void wcrt(vector<struct task> &v_tasks)
{
        int r_curr;
        int r_prev;

        /* sort decreasing priority order 1 -> n */
        sort_inc(v_tasks);

        /* compute WCRT for each task */
        for (unsigned int i = 0; i < v_tasks.size(); i++) {
                /* compute starting r_curr r_prev */
                r_curr = 0;
                r_prev = 0;
                init(v_tasks, v_tasks[i], r_curr, r_prev);
                printf("task.id %d r_curr %d r_prev %d\n", v_tasks[i].id, 
                                r_curr, r_prev);
        }
}

int main(void)
{
        vector<struct task> v_tasks;
        struct task tau_1;
        struct task tau_2;
        struct task tau_3;

        tau_1.c = 3;
        tau_1.t = 7;
        tau_1.d = 7;
        tau_1.p = 1;
        tau_1.r = 0;
        tau_1.id = 0;

        tau_2.c = 3;
        tau_2.t = 12;
        tau_2.d = 12;
        tau_2.p = 2;
        tau_2.r = 0;
        tau_2.id = 1;

        tau_3.c = 5;
        tau_3.t = 20;
        tau_3.d = 20;
        tau_3.p = 3;
        tau_3.r = 0;
        tau_3.id = 2;

        v_tasks.push_back(tau_1);
        v_tasks.push_back(tau_2);
        v_tasks.push_back(tau_3);

        wcrt(v_tasks);

        return 0;
}
