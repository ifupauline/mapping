#include "sched_analysis.h"

int main(void)
{
        int ret;
        vector<struct task> v_tasks;
        struct task tau_0;
        struct task tau_1;
        struct task tau_2;

        tau_0.c = 3;
        tau_0.t = 7;
        tau_0.d = 7;
        tau_0.p = 1;
        tau_0.r = 0;
        tau_0.id = 0;

        tau_1.c = 3;
        tau_1.t = 12;
        tau_1.d = 12;
        tau_1.p = 2;
        tau_1.r = 0;
        tau_1.id = 1;

        tau_2.c = 5;
        tau_2.t = 20;
        tau_2.d = 20;
        tau_2.p = 3;
        tau_2.r = 0;
        tau_2.id = 2;

        v_tasks.push_back(tau_0);
        v_tasks.push_back(tau_1);
        v_tasks.push_back(tau_2);

        ret = wcrt(v_tasks);
        if (ret == SCHED_FAILED)
                printf("Schedulability FAILED\n");
        else 
                printf("\nSchedulability OK\n");

        for (unsigned int i = 0; i < v_tasks.size(); i++)
                printf("tau %d: r: %d t: %d\n",i , v_tasks[i].r, v_tasks[i].t);

        return 0;
}
