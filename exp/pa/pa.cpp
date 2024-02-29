#include "print.h"
#include "ucases.h"
#include "generator.h"
#include "sched_analysis.h"

#define PA1 1
#define PA2 2
#define TCN 5
#define STP 100

struct stats {
        int count_task;
        int count_tc;
};

static int instance_count = 0;

static float int_lvl_pa1 = 0;

static float int_lvl_pa2 = 0;

static void _gen_task(struct task &tau, int i, int color)
{
        int y;
        int real_t;
        float udiff;
        float real_c;
        float real_u;

        while (1) {
                if (color == WHITE) {
                        y  = gen_rand(0, 14);
                        real_c = gen_rand(1000, 50000); /* microsecs */
                        real_u = (real_c/real_t) * PERMILL;
                        real_t = period_waters2015(0, y, DYNAMIC);
                } else {
                        y  = gen_rand(0, 9);
                        real_c = gen_rand(100, 5000); /* microsecs */
                        real_u = (real_c/real_t) * PERMILL;
                        real_t = period_waters2015(0, y, STATIC);
                }

                if (real_u < 10 || real_u > 100)
                        continue;

                tau.u = ceil((real_c/real_t) * PERMILL);
                tau.c = ceil(real_c);
                tau.t = real_t;

                if (tau.c >= tau.t)
                        continue;

                /* if diff too big redo */
                udiff = tau.u - real_u;
                if (udiff > PRECISION)
                        continue;

                tau.r = 0;
                tau.p = i + 1; /* 1 is highest priority */
                tau.task_id = i;
                break;
        }
}

static void _gen_tc(struct tc &tc, int color, int minu, int maxu)
{
        int task_nbr;
        struct task tau;

        while (1) {
                tc = {0};
                tc.tc_idx = 0;
                tc.u = 0;

                if (color == WHITE)
                        task_nbr = gen_rand(1, 12);
                else
                        task_nbr = gen_rand(1, 8); /* ZCU tc */

                tc.weight = gen_rand(1, 3);
                tc.color = color;
                tc.is_frag = NO;
                tc.is_let = NO;
                tc.is_assign = NO;

                for (int i = 0; i < task_nbr; i++) {
                        tau = {0};
                        tau.is_let = NO;
                        tau.tc_id = tc.id;

                        _gen_task(tau, i, color);

                        tc.v_tasks.push_back(tau);
                        tc.u += tau.u;
                }

                if (tc.u < minu || tc.u > maxu)
                        continue;
                else
                        return;
        }
}

static void _cmp_int_lvl(struct core &b, int pa)
{
        int sum;
        float avr;

        copy_back_prio_to_tc(b);

        for (unsigned int i = 0; i < b.v_tcs.size(); i++) {
                avr = 0.0;
                sum = 0;
                for (unsigned int j = 0; j < b.v_tcs[i].v_tasks.size(); j++)
                        sum += b.v_tcs[i].v_tasks[j].p - 1;

                avr = (float)sum / (float)b.v_tcs[i].v_tasks.size();

                if (pa == PA1)
                        int_lvl_pa1 += avr;

                else if (pa == PA2)
                        int_lvl_pa2 += avr;
        }

        if (pa == PA1)
                printf("Instance %-4d PA1 Interference Level: %-.03f\n", 
                                instance_count, int_lvl_pa1);

        else if (pa == PA2)
                printf("Instance %-4d PA2 Interference Level: %-.03f\n\n", 
                                instance_count, int_lvl_pa2);
}

static void _pa1(struct core &b)
{
        int p;

        p = 1;

        b.v_tasks.clear();
        for (unsigned int i = 0; i < b.v_tcs.size(); i++) {
                for (unsigned int j = 0; j < b.v_tcs[i].v_tasks.size(); j++)
                        b.v_tasks.push_back(b.v_tcs[i].v_tasks[j]);
        }

        for (unsigned int i = 0; i < b.v_tasks.size(); i++) {
                b.v_tasks[i].p = p;
                p++;
        }

        _cmp_int_lvl(b, PA1);

        b.flag = wcrt(b.v_tasks);
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

        _cmp_int_lvl(b, PA2);

        b.flag = wcrt(b.v_tasks);
        copy_back_resp_to_tc(b);
}

static struct stats _cmp_unsched(vector<struct core> &v_cores)
{
        struct stats c;
        c.count_tc = 0;

        for (unsigned int i = 0; i < v_cores.size(); i++) {
                for (unsigned int j = 0; j < v_cores[i].v_tcs.size(); j++) {
                        for (unsigned int k = 0; k < v_cores[i].v_tcs[j].v_tasks.size(); k++) {
                                if (v_cores[i].v_tcs[j].v_tasks[k].r > v_cores[i].v_tcs[j].v_tasks[k].t)
                                        c.count_task++;
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

static int _gen_data(vector<struct tc> &v_tcs)
{
        struct tc tc;
        int task_nbr;

        task_nbr = 0;

        for (int i = 0; i < TCN; i++) {
                tc = {0};
                _gen_tc(tc, WHITE, 100, 1000);
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
                b.load += v_tcs[i].u;
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

        int tc_win_pa1;
        int tc_win_pa2;

        int int_count_pa1;
        int int_count_pa2;
        int stp;

        pa1 = {0};
        pa2 = {0};
        cpa1 = {0};
        cpa2 = {0};

        tc_win_pa1 = 0;
        tc_win_pa2 = 0;
        int_count_pa1 = 0;
        int_count_pa2 = 0;

        stp = 1000;

        for (int i = 0; i < stp; i++) {
                cpa1 = {0};
                cpa2 = {0};

                int_lvl_pa1 = 0;
                int_lvl_pa2 = 0;

                v_tcs.clear();
                v_cores.clear();

                instance_count++;
                _gen_data(v_tcs);
                _add(v_tcs, v_cores);

                cpa1 = _assign(v_cores, pa1, PA1);
                cpa2 = _assign(v_cores, pa2, PA2);

                if (cpa1.count_tc < cpa2.count_tc)
                        tc_win_pa1++;

                else if (cpa1.count_tc > cpa2.count_tc)
                        tc_win_pa2++;

                if (int_lvl_pa1 < int_lvl_pa2)
                        int_count_pa1++;
                if (int_lvl_pa2 < int_lvl_pa1)
                        int_count_pa2++;
        }

        printf("\n");
        printf("+===============================================================+\n");
        printf("|%d Executions\n", stp);
        printf("|Total Number of TC: %-3d\n", stp * TCN);
        printf("+===============================================================+\n");
        printf("PA1 Number of Unsched Tasks: %-3d\n", pa1.count_task);
        printf("PA2 Number of Unsched Tasks: %-3d\n", pa2.count_task);
        printf("\n");
        printf("PA1 Number of Unsched TC   : %-3d\n", pa1.count_tc);
        printf("PA2 Number of Unsched TC   : %-3d\n", pa2.count_tc);
        printf("\n");
        printf("PA1 Number of Times lowest Unsched TC   : %d\n", tc_win_pa1);
        printf("PA2 Number of Times lowest Unsched TC   : %d\n", tc_win_pa2);
        printf("\n");
        printf("PA1 Number of Times lowest Interference Level: %d\n", int_count_pa1);
        printf("PA2 Number of Times lowest Interference Level: %d\n", int_count_pa2);

        return 0;
}
