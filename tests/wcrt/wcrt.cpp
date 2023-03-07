#include "sched_analysis.h"

int main(void)
{
        int ret;
        struct task tau_0;
        struct task tau_1;
        struct task tau_2;
        struct task tau_3;
        vector<struct task> v_tasks;

        tau_0.c = 8;
        tau_0.t = 80;
        tau_0.p = 1;
        tau_0.r = 0;
        tau_0.id = 0;

        tau_1.c = 5;
        tau_1.t = 30;
        tau_1.p = 2;
        tau_1.r = 0;
        tau_1.id = 1;

        tau_2.c = 3;
        tau_2.t = 20;
        tau_2.p = 3;
        tau_2.r = 0;
        tau_2.id = 2;

        tau_3.c = 3;
        tau_3.t = 50;
        tau_3.p = 4;
        tau_3.r = 0;
        tau_3.id = 3;

        v_tasks.push_back(tau_0);
        v_tasks.push_back(tau_1);
        v_tasks.push_back(tau_2);
        v_tasks.push_back(tau_3);

        ret = wcrt(v_tasks);
        if (ret == SCHED_FAILED)
                printf("Schedulability FAILED\n");
        else 
                printf("\nSchedulability OK\n");

        for (unsigned int i = 0; i < v_tasks.size(); i++)
                printf("tau %d: p: %d r: %d c: %d t: %d\n", v_tasks[i].id, 
                                v_tasks[i].p , v_tasks[i].r, 
                                v_tasks[i].c, v_tasks[i].t);
        return 0;
}
