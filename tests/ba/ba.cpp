#include "print.h"
#include "generator.h"
#include "sched_analysis.h"

struct count {
        int count_task;
        int count_tc;
};

static void _base_assignment(struct core &b)
{
        int p;

        p = 1;

        /* no need to check for TC belonging to the same TC coze of ordering */
        sort_inc_task_id(b.v_tasks);

        for (unsigned int i = 0; i < b.v_tasks.size(); i++) {
                b.v_tasks[i].p = p;
                p++;
        }
        b.flag = wcrt(b.v_tasks);
        copy_back_prio_to_tc(b);
        copy_back_resp_to_tc(b);
}

static void _naiv_assignment(struct core &b)
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

static struct count _cmp_unsched(vector<struct core> &v_cores)
{
        struct count c;
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

int main(void)
{
        struct tc tc;
        struct core b;
        struct context ctx;
        vector<struct tc> v_tcs;
        vector<struct core> v_cores;

        struct count c = {0};

        int base_count_task = 0;
        int base_count_tc = 0;

        int naiv_count_task = 0;
        int naiv_count_tc = 0;

        for (unsigned int z; z < 100; z++) {
                b = {0};
                ctx = {0};
                v_tcs.clear();
                v_cores.clear();
                for (int i = 0; i < 5; i++) {
                        tc = {0};
                        create_tc(tc, WHITE, 50, 500);
                        v_tcs.push_back(tc);
                }
                sort_dec_tc_size(v_tcs);

                for (unsigned int i = 0; i < v_tcs.size(); i++) {
                        v_tcs[i].id = i;
                        b.v_tcs.push_back(v_tcs[i]);
                        b.load += v_tcs[i].size;
                }

                v_cores.push_back(b);

                sort_inc_core_load_rem(v_cores);
                copy_v_tc_to_v_tasks_with_pos(v_cores);

                for (unsigned int i = 0; i < v_cores.size(); i++) {
                        if (v_cores[i].load == 0)
                                continue;
                        else
                                _base_assignment(v_cores[i]);
                }

                print_cores(v_cores, ctx);
                c = _cmp_unsched(v_cores);
                base_count_task += c.count_task;
                base_count_tc += c.count_tc;

                c = {0};

                sort_inc_core_load_rem(v_cores);
                copy_v_tc_to_v_tasks_with_pos(v_cores);

                for (unsigned int i = 0; i < v_cores.size(); i++) {
                        if (v_cores[i].load == 0)
                                continue;
                        else
                                _naiv_assignment(v_cores[i]);
                }
                print_cores(v_cores, ctx);
                c = _cmp_unsched(v_cores);
                naiv_count_task += c.count_task;
                naiv_count_tc += c.count_tc;
        }

        printf("naiv_count_task: %d\n", naiv_count_task);
        printf("base_count_task: %d\n", base_count_task);
        printf("naiv_count_tc: %d\n", naiv_count_tc);
        printf("base_count_tc: %d\n", base_count_tc);

        return 0;
}
