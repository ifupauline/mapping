#include "sched_analysis.h"

int main(void)
{
        int ret;
        vector<struct task> v_tasks;
        struct task tau_0;
        struct task tau_1;
        struct task tau_2;
        struct task tau_3;
        struct task tau_4;

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

        tau_2.c = 1;
        tau_2.t = 20;
        tau_2.d = 20;
        tau_2.p = 3;
        tau_2.r = 0;
        tau_2.id = 2;

        tau_3.c = 2;
        tau_3.t = 40;
        tau_3.d = 40;
        tau_3.p = 4;
        tau_3.r = 0;
        tau_3.id = 3;

        tau_4.c = 4;
        tau_4.t = 40;
        tau_4.d = 40;
        tau_4.p = 5;
        tau_4.r = 0;
        tau_4.id = 4;

        v_tasks.push_back(tau_0);
        v_tasks.push_back(tau_1);
        v_tasks.push_back(tau_2);
        v_tasks.push_back(tau_3);
        v_tasks.push_back(tau_4);

        ret = wcrt(v_tasks);
        if (ret == SCHED_FAILED)
                printf("SCHEDULABILITY FAILED\n");
        else 
                printf("\nSCHEDULABILITY OK\n");

        return 0;
}
