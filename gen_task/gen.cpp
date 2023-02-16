#include "stdio.h"

#include <vector>
#include <random>

#define N 100
#define CAPACITY 100

#define MINTASKNBR 2
#define MAXTASKNBR 10

#define MINU 1
#define MAXU CAPACITY/6

#define MINWCET 1
#define MAXWCET 10

#define PERCENT 100

using namespace std;

struct task {
        int c;
        int t;
        int d;
        int u;
        int p;
        int r;
        int id;
};

struct task_chain {
        int u;
        vector<struct task> v_tasks;
};

static int gen_rand(int min, int max) 
{
        random_device rd;
        mt19937 gen(rd());
        uniform_int_distribution<> distr(min, max);

        return distr(gen);
}

int main(void)
{
        int task_nbr;
        vector<struct task> v_tasks;
        vector<struct task_chain> v_tc;
        int count = 0;
        int count2 = 0;
        int total_task = 0;

        /* generate number of tasks wished in the chain */

redo:        for (int j = 0; j < N; j++) {
                     task_nbr = gen_rand(MINTASKNBR, MAXTASKNBR);
                     /* generate size of tasks wcet and period */
                     struct task_chain tc;
                     tc.u = 0;
                     for (int i = 0; i < task_nbr; i++) {
                             struct task tau;
                             tau.u = gen_rand(MINU, MAXU);
                             tau.c = gen_rand(MINWCET, MAXWCET);
                             tau.t = ((float)tau.c/(float)tau.u) * PERCENT;
                             tc.v_tasks.push_back(tau);
                             tc.u += tau.u;
                     }
                     v_tc.push_back(tc);
                     printf("\ntask_nbr: %d\n", task_nbr);
                     printf("tc.u: %d\n", tc.u);

                     if (tc.u > CAPACITY) {
                             count++;
                     }

                     if (tc.u > CAPACITY - 20) {
                             count2++;
                     }

                     for (unsigned int i = 0; i < tc.v_tasks.size(); i++) {
                             printf("tc.v_tasks[%d].u: %d  c: %d  t: %d\n", i, tc.v_tasks[i].u, tc.v_tasks[i].c, tc.v_tasks[i].t);
                             total_task++;
                     }
             }

             if (count > 0) {
                     count = 0;
                     count2 = 0;
                     total_task = 0;
                     goto redo;
             }

             //if (count2 < 20) {
             //        count = 0;
             //        count2 = 0;
             //        goto redo;
             //}

             printf("number of tc > capacity: %d\n", count);
             printf("number of tc > phi: %d\n", count2);
             printf("total task nbr: %d\n", total_task);

             return 0;
}



































