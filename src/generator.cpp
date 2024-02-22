#include "ucases.h"
#include "generator.h"
#include "sched_analysis.h"

static void _assign_ids(vector<struct tc> &v_tcs)
{
        unsigned int uniq_id;

        uniq_id = 0;

        for (unsigned int i = 0; i < v_tcs.size(); i++) {
                v_tcs[i].id = i;
                for (unsigned int j = 0; j < v_tcs[i].v_tasks.size(); j++) {
                        v_tcs[i].v_tasks[j].uniq_id = uniq_id;
                        uniq_id++;
                }
        }
}

static void _check_prm(struct params &prm)
{
        if (prm.s < 150 || prm.s > PHI) {
                printf("Invalid params: prm.s rule -> [%d <= s <= %d]\n\n", 
                                150,  PHI);
                exit(0);
        }
}

static int _cmp_tc_u(vector<struct tc> &v_tcs, struct context &ctx)
{
        ctx.gamma_u = 0;

        for (unsigned int i = 0; i < v_tcs.size(); i++)
                ctx.gamma_u += v_tcs[i].size;

        ctx.cores_min = ceil(ctx.gamma_u / PHI);

        return 0;
}

static void _create_task(struct task &tau, int i, int color)
{
        int y;
        int real_t;
        float udiff;
        float real_c;
        float real_u;

        while (1) {
                if (color == WHITE) {
                        y  = gen_rand(0, 7);
                        real_t = period_waters2015(0, y, DYNAMIC);
                } else {
                        y  = gen_rand(0, 5);
                        real_t = period_waters2015(0, y, STATIC);
                }

                real_c = gen_rand(1, 30000); /* microsecs */
                real_u = (real_c/real_t) * PERMILL;

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

void create_tc(struct tc &tc, int color, int minu, int maxu)
{
        int task_nbr;
        struct task tau;

        while (1) {
                tc = {0};
                tc.tc_idx = 0;
                tc.size = 0;

                if (color == WHITE || color == RED)
                        task_nbr = gen_rand(2, 15);
                else
                        task_nbr = gen_rand(2, 6); /* ZCU tc */

                tc.comcost = gen_rand(1, 3);
                tc.color = color;
                tc.is_frag = NO;
                tc.is_let = NO;
                tc.is_allocated = NO;

                for (int i = 0; i < task_nbr; i++) {
                        tau = {0};
                        tau.is_let = NO;
                        tau.tc_id = tc.id;

                        _create_task(tau, i, color);

                        tc.v_tasks.push_back(tau);
                        tc.size += tau.u;
                }

                if (tc.size < minu || tc.size > maxu)
                        continue;
                else
                        return;
        }
}

static int _gen_app(vector<struct tc> &v_tcs, struct params &prm, 
                struct context &ctx)
{
        printf("\n\n");
        printf("+=====================================+\n");
        printf("| CASE-STUDY INSTANCE GENERATION      |\n");
        printf("+=====================================+\n");

        struct tc tc;

        /* red */
        tc = {0};
        create_tc(tc, RED, 100, 500);
        v_tcs.push_back(tc);

        tc = {0};
        create_tc(tc, RED, 100, 500);
        v_tcs.push_back(tc);

        tc = {0};
        create_tc(tc, RED, 100, 500);
        v_tcs.push_back(tc);

        tc = {0};
        create_tc(tc, RED, 100, 500);
        v_tcs.push_back(tc);

        /* blue */
        tc = {0};
        create_tc(tc, BLUE, 100, 250);
        v_tcs.push_back(tc);
        tc = {0};
        create_tc(tc, BLUE, 100, 250);
        v_tcs.push_back(tc);

        /* yellow */
        tc = {0};
        create_tc(tc, YELLOW, 100, 250);
        v_tcs.push_back(tc);
        tc = {0};
        create_tc(tc, YELLOW, 100, 250);
        v_tcs.push_back(tc);

        /* green */
        tc = {0};
        create_tc(tc, GREEN, 100, 250);
        v_tcs.push_back(tc);
        tc = {0};
        create_tc(tc, GREEN, 100, 250);
        v_tcs.push_back(tc);

        /* cyan */
        tc = {0};
        create_tc(tc, CYAN, 100, 250);
        v_tcs.push_back(tc);
        tc = {0};
        create_tc(tc, CYAN, 100, 250);
        v_tcs.push_back(tc);

        /* purple */
        tc = {0};
        create_tc(tc, PURPLE, 100, 250);
        v_tcs.push_back(tc);
        tc = {0};
        create_tc(tc, PURPLE, 100, 250);
        v_tcs.push_back(tc);

        /* white */
        for (int i = 0; i < 8; i++) {
                tc = {0};
                create_tc(tc, WHITE, 100, 1000);
                v_tcs.push_back(tc);
        }
        sort_dec_tc_size(v_tcs);
        _assign_ids(v_tcs);

        ctx.prm.n = v_tcs.size();
        prm.n = v_tcs.size();

        for (unsigned int i = 0; i < v_tcs.size(); i++)
                v_tcs[i].gcd = cmp_gcd(v_tcs[i].v_tasks);

        _cmp_tc_u(v_tcs, ctx);

        return 0;
}

void cut(vector<struct tc> &v_tcs, struct context &ctx)
{
        int idx;
        int ret;
        int u_sum;
        struct tc tc;
        vector<struct task> v_tmp;
        vector<struct tc> v_new_tcs;
        vector<struct tc> v_tmp_tcs;

        idx = 0;
        ret = -1;
        u_sum = 0;
        ctx.frags_count = 0;

        /* store tcs */
        for (unsigned int i = 0; i < v_tcs.size(); i++)
                v_tmp_tcs.push_back(v_tcs[i]);

        /* partitioning */
        for (unsigned int i = 0; i < v_tmp_tcs.size(); i++) {
                idx = 0;
                for (unsigned int j = 0; j < v_tmp_tcs[i].v_tasks.size(); j++) {
                        ret = -1;
                        u_sum += v_tmp_tcs[i].v_tasks[j].u;
                        v_tmp_tcs[i].v_tasks[j].tc_id = v_tmp_tcs[i].id;
                        v_tmp.push_back(v_tmp_tcs[i].v_tasks[j]);
                        ret = wcrt(v_tmp);
                        if (ret == SCHED_FAILED)  {
                                u_sum -= v_tmp_tcs[i].v_tasks[j].u;
                                v_tmp.pop_back();
                                tc = {0};
                                tc.id = v_tmp_tcs[i].id;
                                tc.tc_idx = idx;
                                tc.size = u_sum;
                                tc.comcost = v_tmp_tcs[i].comcost;
                                tc.color = v_tmp_tcs[i].color;
                                tc.is_let = NO;
                                tc.is_frag = YES;
                                tc.is_allocated = NO;
                                tc.v_tasks = v_tmp;
                                v_new_tcs.push_back(tc);
                                j--;
                                idx++;
                                u_sum = 0;
                                v_tmp.clear();
                                continue;
                        }

                        /* only for the waters2019 task-chain */
                        if (i == 0 && u_sum > PHI) {
                                u_sum -= v_tmp_tcs[i].v_tasks[j].u;
                                v_tmp.pop_back();
                                tc = {0};
                                tc.id = v_tmp_tcs[i].id;
                                tc.tc_idx = idx;
                                tc.size = u_sum;
                                tc.comcost = v_tmp_tcs[i].comcost;
                                tc.color = v_tmp_tcs[i].color;
                                tc.is_let = NO;
                                tc.is_frag = YES;
                                tc.is_allocated = NO;
                                tc.v_tasks = v_tmp;
                                v_new_tcs.push_back(tc);
                                j--;
                                idx++;
                                u_sum = 0;
                                v_tmp.clear();
                                continue;
                        }

                        /* -10 to account for increase of let size */
                        if (i > 0 && u_sum > ctx.prm.s - 10) {
                                u_sum -= v_tmp_tcs[i].v_tasks[j].u;
                                v_tmp.pop_back();
                                tc = {0};
                                tc.id = v_tmp_tcs[i].id;
                                tc.tc_idx = idx;
                                tc.size = u_sum;
                                tc.comcost = v_tmp_tcs[i].comcost;
                                tc.color = v_tmp_tcs[i].color;
                                tc.is_let = NO;
                                tc.is_frag = YES;
                                tc.is_allocated = NO;
                                tc.v_tasks = v_tmp;
                                v_new_tcs.push_back(tc);
                                j--;
                                idx++;
                                u_sum = 0;
                                v_tmp.clear();
                                continue;
                        }
                        /* add last fragment */
                        if (j == v_tmp_tcs[i].v_tasks.size() - 1) {
                                tc = {0};
                                tc.id = v_tmp_tcs[i].id;
                                tc.tc_idx = idx;
                                tc.size = u_sum;
                                tc.comcost = v_tmp_tcs[i].comcost;
                                tc.color = v_tmp_tcs[i].color;
                                tc.is_let = NO;
                                tc.is_frag = YES;
                                tc.is_allocated = NO;
                                tc.v_tasks = v_tmp;
                                v_new_tcs.push_back(tc);
                                u_sum = 0;
                                v_tmp.clear();
                                ctx.frags_count++;
                                continue;
                        }
                }
        }
        v_tcs.clear();
        v_tcs = v_new_tcs;
        sort_dec_tc_size(v_tcs);
        for (unsigned int i = 0; i < v_tcs.size(); i++)
                v_tcs[i].gcd = cmp_gcd(v_tcs[i].v_tasks);


        printf("Initial Number of TC:   %d\n", ctx.prm.n);
        printf("Current Number of TC:   %ld\n", v_tcs.size());
        printf("Number of TC Cuts: %d\n", ctx.frags_count);
}

int gen_rand(int min, int max) 
{
        random_device rd;
        mt19937 gen(rd());
        uniform_int_distribution<> distr(min, max);

        return distr(gen);
}

void input_prm(int argc, char **argv, struct params &prm)
{
        if (argc != 2) {
                printf("Wrong Number of Arguments!\n");
                exit(0);
        }
        prm.s = atoi(argv[1]);
        _check_prm(prm);
}

void init_ctx(vector<struct tc> &v_tcs, struct params &prm, 
                struct context &ctx)
{
        float part_time;

        ctx.prm = prm;
        ctx.cores_count = 0;
        ctx.alloc_count = 0;
        ctx.tasks_count = 0;
        ctx.sched_ok_count = 0;
        ctx.sched_failed_count = 0;
        ctx.gamma_u = 0;
        ctx.tcs_count = ctx.prm.n;

        part_time = ctx.p.part_time;
        ctx.p = {0};
        ctx.p.part_time = part_time;
}

void gen_app(vector<struct tc> &v_tcs, struct params &prm, 
                struct context &ctx)
{
        ctx.prm = prm;
        _gen_app(v_tcs, prm, ctx);
}

void gen_arch(vector<struct core> &v_cores, struct context &ctx)
{
        for (int i = 0; i < 8; i++)
                add_core(v_cores, RED, 1, ctx);

        for (int i = 0; i < 4; i++) {
                add_core(v_cores, BLUE, 1, ctx);
                add_core(v_cores, YELLOW, 1, ctx);
                add_core(v_cores, GREEN, 1, ctx);
                add_core(v_cores, CYAN, 1, ctx);
                add_core(v_cores, PURPLE, 1, ctx);
        }
        ctx.init_cores_count = v_cores.size();
}
