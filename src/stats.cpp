#include "stats.h"
#include "sched_analysis.h"

static void _count_cores(vector<struct core> &v_cores, struct context &ctx)
{
        ctx.cores_count = 0;

        for (unsigned int i = 0; i < v_cores.size(); i++) {
                if (v_cores[i].is_empty == YES)
                        continue;
                else
                        ctx.cores_count++;

                if (v_cores[i].color == RED)
                        ctx.pcu_cores_count++;

                if (v_cores[i].color != RED)
                        ctx.zcu_cores_count++;

                for (unsigned int j = 0; j < v_cores[i].v_tasks.size(); j++)
                        ctx.tasks_count++;
        }
}

static void _execution_time(struct context &ctx)
{
        ctx.p.exec_time = ctx.p.part_time+ ctx.p.assi_time + ctx.p.schd_time + 
                ctx.p.disp_time + ctx.p.swap_time;

        ctx.p.plac_time = ctx.p.disp_time + ctx.p.swap_time;
}

static void _schedulability_rate(struct context &ctx)
{
        ctx.p.sched_rate_allo = ctx.p.sched_rate_allo * PERCENT;
        ctx.p.swap_gain = (ctx.p.sched_rate_swap * PERCENT) - ctx.p.sched_rate_allo;
        ctx.p.disp_gain = (ctx.p.sched_rate_disp * PERCENT) - (ctx.p.sched_rate_swap * PERCENT);
        ctx.p.plac_gain = ctx.p.swap_gain + ctx.p.disp_gain;
}

static void _utilization_rate(vector<struct core> &v_cores, struct context &ctx)
{
        ctx.p.letu = 0.0;
        ctx.p.appu = 0.0;
        ctx.p.unuu = 0.0;
        ctx.p.sysu = 0.0;

        for (unsigned int i = 0; i < v_cores.size(); i++) {
                if (v_cores[i].is_empty == YES)
                        continue;
                ctx.p.appu += v_cores[i].load;
                ctx.p.unuu += (float)v_cores[i].load_rem;
                for (unsigned int j = 0; j < v_cores[i].v_tcs.size(); j++) {
                        if (v_cores[i].v_tcs.size() == 1 && 
                                        v_cores[i].v_tcs[j].is_let == YES)
                                continue;
                        if (v_cores[i].v_tcs[j].is_let == YES) 
                                ctx.p.letu += v_cores[i].v_tcs[j].u;
                }
        }
        /* substract let utilization to global sys */
        ctx.p.appu -= ctx.p.letu;
        ctx.p.letu /= PERMILL;
        ctx.p.appu /= PERMILL;
        ctx.p.unuu /= PERMILL;
        ctx.p.sysu = ctx.cores_count * ((float)PHI / (float)PERMILL);
}

static void _verify_pa(vector<struct core> &v_cores)
{
        int ret;

        vector<struct tc> v_tc;
        vector<int> v_int;

        ret = 0;
        for (unsigned int i = 0; i < v_cores.size(); i++) {
                for (unsigned int j = 0; j < v_cores[i].v_tcs.size(); j++) {
                        if (v_cores[i].v_tcs[j].is_let == YES)
                                continue;
                        v_int.push_back(v_cores[i].v_tcs[j].id);
                        v_tc.push_back(v_cores[i].v_tcs[j]);
                }
                ret = get_duplicata(v_int);

                if (ret > -1) {
                        vector<struct task> v_tasks;
                        for (unsigned int j = 0; j < v_tc.size(); j++) {
                                for (unsigned int k = 0; k < v_tc[j].v_tasks.size(); k++) {
                                        if (v_tc[j].v_tasks[k].tc_id == ret)
                                                v_tasks.push_back(v_tc[j].v_tasks[k]);
                                }
                        }
                        sort_inc_task_priority(v_tasks);

                        for (unsigned int j = 0; j < v_tasks.size(); j++) {
                                if (j == 1) {
                                        if (v_tasks[j].task_id < v_tasks[j - 1].task_id) {
                                                printf("ERR! PA CORE %d\n", v_cores[i].id);
                                                printf("TC %d tau: %d p: %d\n", 
                                                                v_tasks[j].tc_id, 
                                                                v_tasks[j].task_id, 
                                                                v_tasks[j].p);
                                                printf("TC %d tau: %d p: %d\n", 
                                                                v_tasks[j - 1].tc_id, 
                                                                v_tasks[j - 1].task_id, 
                                                                v_tasks[j - 1].p);
                                                exit(0);
                                        }
                                }
                        }
                }
                v_tc.clear();
                v_int.clear();
        }
}

float sched_rate(vector<struct core> &v_cores, struct context &ctx)
{
        float sched_rate;

        sched_rate = 0.0;
        ctx.sched_ok_count = 0;
        ctx.sched_failed_count = 0;

        for (unsigned int i = 0; i < v_cores.size(); i++) {
                if (v_cores[i].flag == SCHED_OK)
                        ctx.sched_ok_count++;
        }

        for (unsigned int i = 0; i < v_cores.size(); i++) {
                if (v_cores[i].flag == SCHED_FAILED)
                        ctx.sched_failed_count++;
        }

        sched_rate = (float)ctx.sched_ok_count / (float)(ctx.sched_ok_count + ctx.sched_failed_count);

        return sched_rate;
}

void stats(vector<struct core> &v_cores, vector<struct tc> &v_tcs, 
                struct context &ctx)
{
        _verify_pa(v_cores);
        _schedulability_rate(ctx);
        _execution_time(ctx);
        _utilization_rate(v_cores, ctx);
        _count_cores(v_cores, ctx);
}
