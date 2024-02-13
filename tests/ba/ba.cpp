#include "print.h"
#include "generator.h"
#include "sched_analysis.h"

#define STP 100
#define PA1 1
#define PA2 2

struct stats {
        int count_task;
        int count_tc;
};

static void _pa1(struct core &b)
{
        int p;

        p = 1;

        b.v_tasks.clear();
        for (unsigned int i = 0; i < b.v_tcs.size(); i++) {
                for (unsigned int j = 0; j < b.v_tcs[i].v_tasks.size(); j++) {
                        b.v_tasks.push_back(b.v_tcs[i].v_tasks[j]);

                }
        }

        for (unsigned int i = 0; i < b.v_tasks.size(); i++) {
                b.v_tasks[i].p = p;
                p++;
        }
        b.flag = wcrt(b.v_tasks);
        copy_back_prio_to_tc(b);
        copy_back_resp_to_tc(b);
}

static void _pa2(struct core &b)
{
        int p;

        p = 1;

        sort_inc_task_id(b.v_tasks);

        for (unsigned int i = 0; i < b.v_tasks.size(); i++) {
                b.v_tasks[i].p = p;
                p++;
        }
        b.flag = wcrt(b.v_tasks);
        copy_back_prio_to_tc(b);
        copy_back_resp_to_tc(b);
}

static struct stats _cmp_unsched(vector<struct core> &v_cores)
{
        struct stats c;
        c.count_task = 0;
        c.count_tc = 0;

        for (unsigned int i = 0; i < v_cores.size(); i++) {
                for (unsigned int j = 0; j < v_cores[i].v_tcs.size(); j++) {
                        for (unsigned int k = 0; k < v_cores[i].v_tcs[j].v_tasks.size(); k++) {
                                if (v_cores[i].v_tcs[j].v_tasks[k].r > v_cores[i].v_tcs[j].v_tasks[k].t) {
                                        c.count_task++;
                                }
                        }
                }
        }

        for (unsigned int i = 0; i < v_cores.size(); i++) {
                for (unsigned int j = 0; j < v_cores[i].v_tcs.size(); j++) {
                        for (unsigned int k = 0; k < v_cores[i].v_tcs[j].v_tasks.size(); k++) {
                                if (v_cores[i].v_tcs[j].v_tasks[k].r > v_cores[i].v_tcs[j].v_tasks[k].t) {
                                        c.count_tc++;
                                        break;
                                }
                        }
                }
        }
        return c;
}

static int _create_data(vector<struct tc> &v_tcs)
{
        struct tc tc;
        int task_nbr;

        task_nbr = 0;

        for (int i = 0; i < 5; i++) {
                tc = {0};
                create_tc(tc, WHITE, 50, 500);
                v_tcs.push_back(tc);
                task_nbr += tc.v_tasks.size();
        }
        sort_dec_tc_size(v_tcs);
        return task_nbr;
}

static void _add(vector<struct tc> &v_tcs, vector<struct core> &v_cores)
{
        struct core b;
        b = {0};

        for (unsigned int i = 0; i < v_tcs.size(); i++) {
                v_tcs[i].id = i;
                b.v_tcs.push_back(v_tcs[i]);
                b.load += v_tcs[i].size;
        }

        v_cores.push_back(b);

        sort_inc_core_load_rem(v_cores);
        copy_v_tc_to_v_tasks_with_pos(v_cores);
}

static struct stats _assign(vector<struct core> &v_cores, struct stats &pa, int a)
{
        struct stats c;
        struct context ctx;
        c = {0};
        ctx = {0};

        for (unsigned int i = 0; i < v_cores.size(); i++) {
                if (v_cores[i].load == 0)
                        continue;
                else if (a == PA1)
                        _pa1(v_cores[i]);
                else
                        _pa2(v_cores[i]);
        }
        //print_cores(v_cores, ctx);
        c = _cmp_unsched(v_cores);
        pa.count_task += c.count_task;
        pa.count_tc += c.count_tc;

        sort_inc_core_load_rem(v_cores);
        copy_v_tc_to_v_tasks_with_pos(v_cores);

        return c;
}

int main(void)
{
        vector<struct tc> v_tcs;
        vector<struct core> v_cores;

        struct stats pa1;
        struct stats pa2;
        struct stats cpa1;
        struct stats cpa2;

        int task_win_pa1;
        int task_win_pa2;
        int tc_win_pa1;
        int tc_win_pa2;

        int task_nbr;
        int stp;

        pa1 = {0};
        pa2 = {0};
        cpa1 = {0};
        cpa2 = {0};

        task_win_pa1 = 0;
        task_win_pa2 = 0;
        tc_win_pa1 = 0;
        tc_win_pa2 = 0;

        task_nbr = 0;
        stp = 100;

        for (int i = 0; i < stp; i++) {
                cpa1 = {0};
                cpa2 = {0};
                v_tcs.clear();
                v_cores.clear();

                task_nbr += _create_data(v_tcs);
                _add(v_tcs, v_cores);

                cpa1 = _assign(v_cores, pa1, PA1);
                cpa2 = _assign(v_cores, pa2, PA2);

                if (cpa1.count_task > cpa2.count_task)
                        task_win_pa1++;
                else
                        task_win_pa2++;

                if (cpa1.count_tc > cpa2.count_tc)
                        tc_win_pa1++;
                else
                        tc_win_pa2++;
        }

        printf("Total Number of Tasks over %d Executions: %-3d\n", stp, task_nbr);
        printf("PA1 Total Number of Unsched Tasks over %d Executions: %-3d\n", stp, pa1.count_task);
        printf("PA2 Total Number of Unsched Tasks over %d Executions: %-3d\n", stp, pa2.count_task);
        printf("PA1 Number of Times with more Unsched Tasks than PA2 over %d Executions: %d\n", stp, task_win_pa1);
        printf("PA2 Number of Times with more Unsched Tasks than PA1 over %d Executions: %d\n", stp, task_win_pa2);
        printf("\n");
        printf("PA1 Total Number of Unsched TC over %d Executions: %-3d\n", stp, pa1.count_tc);
        printf("PA2 Total Number of Unsched TC over %d Executions: %-3d\n", stp, pa2.count_tc);
        printf("PA1 Number of Times with more Unsched TC than PA2 over %d Executions: %d\n", stp, tc_win_pa1);
        printf("PA2 Number of Times with more Unsched TC than PA1 over %d Executions: %d\n", stp, tc_win_pa2);

        return 0;
}
