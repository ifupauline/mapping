#include "ucases.h"
#include "generator.h"
#include "sched_analysis.h"

static int counter = 0;

int gen_rand(int min, int max) 
{
        random_device rd;
        mt19937 gen(rd());
        uniform_int_distribution<> distr(min, max);

        return distr(gen);
}

void gen_waters2019(vector<struct tc> &v_tcs, struct params &prm, 
                struct context &ctx)
{
        printf("\n\n");
        printf("+=====================================+\n");
        printf("| CASE-STUDY WATERS2019 CHALLENGE     |\n");
        printf("+=====================================+\n");

        struct tc tc_red;

        ctx.prm = prm;

        create_waters2019(tc_red);
        v_tcs.push_back(tc_red);

        sort_dec_tc_size(v_tcs);
        assign_ids(v_tcs);

        ctx.prm.m = v_tcs.size();
        prm.m = v_tcs.size();

        for (unsigned int i = 0; i < v_tcs.size(); i++)
                v_tcs[i].gcd = gcd(v_tcs[i].v_tasks);

}

void gen_app(vector<struct tc> &v_tcs, struct params &prm, 
                struct context &ctx)
{
        printf("\n\n");
        printf("+=====================================+\n");
        printf("| CASE-STUDY INSTANCE GENERATION      |\n");
        printf("+=====================================+\n");

        struct tc tc_red;
        struct tc tc_blue;
        struct tc tc_yellow;
        struct tc tc_green;
        struct tc tc_cyan;
        struct tc tc_purple;
        struct tc tc_white;

        ctx.prm = prm;

        /* static */
        for (unsigned int i = 0; i < 2; i++) {
                tc_red = {0};
                tc_blue = {0};
                tc_yellow = {0};
                tc_green = {0};
                tc_cyan = {0};
                tc_purple = {0};
                gen_tc(tc_red, RED, 1, 500);
                gen_tc(tc_blue, BLUE, 1, 500);
                gen_tc(tc_yellow, YELLOW, 1, 500);
                gen_tc(tc_green, GREEN, 1, 500);
                gen_tc(tc_cyan, CYAN, 1, 500);
                gen_tc(tc_purple, PURPLE, 1, 500);
                v_tcs.push_back(tc_red);
                v_tcs.push_back(tc_blue);
                v_tcs.push_back(tc_yellow);
                v_tcs.push_back(tc_green);
                v_tcs.push_back(tc_cyan);
                v_tcs.push_back(tc_purple);
        }

        /* dynamic */
        for (int i = 0; i < 12; i++) {
                tc_white = {0};
                gen_tc(tc_white, WHITE, 1, 1000);
                v_tcs.push_back(tc_white);
        }
        sort_dec_tc_size(v_tcs);
        assign_ids(v_tcs);

        ctx.prm.m = v_tcs.size();
        prm.m = v_tcs.size();

        for (unsigned int i = 0; i < v_tcs.size(); i++)
                v_tcs[i].gcd = gcd(v_tcs[i].v_tasks);
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
        ctx.p_arch = v_cores.size();
}

void gen_task(struct task &tau, int i, int color)
{
        int y;
        int real_t;
        float udiff;
        float real_c;
        float real_u;

        y = 0;
        udiff = 0;
        real_c = 0;
        real_u = 0;
        real_t = 0;

        while (1) {
                /* the period must be drawn before computing the utilization */
                if (color == WHITE) {
                        y  = gen_rand(0, 14);
                        real_c = gen_rand(10, 10000); /* microsecs */
                        real_t = period_waters2015(0, y, DYNAMIC);
                        real_u = (real_c/real_t) * PERMILL;

                } else {
                        y  = gen_rand(0, 9);
                        real_c = gen_rand(10, 1000); /* microsecs */
                        real_t = period_waters2015(0, y, STATIC);
                        real_u = (real_c/real_t) * PERMILL;
                }

                if (EXP == EXP1) {
                        if (real_u > PHI)
                                continue;
                }

                if (EXP == EXP2) {
                        if (real_u > PHI)
                                continue;
                }

                if (EXP == EXP3) {
                        if (real_u > PHI/8)
                                continue;
                }

                if (EXP == EXP4) {
                        if (real_u > PHI/8)
                                continue;
                }

                if (EXP == EXPM) {
                        if (real_u > PHI)
                                continue;
                }

                if (EXP == EXPP) {
                        if (real_u > PHI)
                                continue;
                }

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

void gen_tc(struct tc &tc, int color, int minu, int maxu)
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
                        task_nbr = gen_rand(1, 8);

                tc.color = color;
                tc.is_frag = NO;
                tc.is_let = NO;
                tc.is_assign = NO;

                for (int i = 0; i < task_nbr; i++) {
                        tau = {0};
                        tau.is_let = NO;
                        tau.tc_id = tc.id;

                        gen_task(tau, i, color);

                        tc.v_tasks.push_back(tau);
                        tc.u += tau.u;
                }

                if (tc.u < minu || tc.u > maxu)
                        continue;
                else {
                        counter++;
                        //printf("%d\n", counter);
                        return;
                }
        }
}

/* close the current fragment of task-chain src (if any) and start a new one */
static void _flush_fragment(struct tc &src, vector<struct task> &v_tmp,
                int &u_sum, int &idx, vector<struct tc> &v_new_tcs)
{
        struct tc tc;

        if (v_tmp.empty())
                return;

        tc = {0};
        tc.id = src.id;
        tc.tc_idx = idx;
        tc.u = u_sum;
        tc.color = src.color;
        tc.is_let = NO;
        tc.is_frag = YES;
        tc.is_assign = NO;
        tc.v_tasks = v_tmp;
        v_new_tcs.push_back(tc);

        idx++;
        u_sum = 0;
        v_tmp.clear();
}

/*
 * Partitioning step, Algorithm 1. Each topologically ordered task-chain is
 * cut into contiguous fragments such that a fragment is schedulable on its
 * own (Constraint 3.4) and, unless it is a single task, its size does not
 * exceed sigma (Constraint 3.3, with a margin of 10 for the LET task).
 * The number of cuts of a chain is its number of fragments minus one.
 */
void cut(vector<struct tc> &v_tcs, struct context &ctx)
{
        int idx;
        int ret;
        int u_sum;
        int count_e;
        int limit;
        size_t frag_start;
        struct task tau;
        vector<struct task> v_tmp;
        vector<struct tc> v_new_tcs;
        vector<struct tc> v_tmp_tcs;

        idx = 0;
        ret = -1;
        u_sum = 0;
        ctx.k = 0;
        count_e = 0;
        ctx.k_max = 0;
        limit = ctx.prm.s - 10;

        /* compute k max */
        for (unsigned int i = 0; i < v_tcs.size(); i++) {
                count_e = 0;
                if (v_tcs[i].is_let == YES)
                        continue;

                count_e = v_tcs[i].v_tasks.size();
                count_e--;
                ctx.k_max += count_e;
        }

        /* store tcs */
        for (unsigned int i = 0; i < v_tcs.size(); i++)
                v_tmp_tcs.push_back(v_tcs[i]);

        /* partitioning */
        for (unsigned int i = 0; i < v_tmp_tcs.size(); i++) {
                idx = 0;
                u_sum = 0;
                v_tmp.clear();
                frag_start = v_new_tcs.size();

                for (unsigned int j = 0; j < v_tmp_tcs[i].v_tasks.size(); j++) {
                        ret = -1;
                        tau = v_tmp_tcs[i].v_tasks[j];
                        tau.tc_id = v_tmp_tcs[i].id;

                        /* task alone bigger than sigma: it becomes a fragment */
                        if (tau.u > limit) {
                                _flush_fragment(v_tmp_tcs[i], v_tmp, u_sum, idx,
                                                v_new_tcs);
                                v_tmp.push_back(tau);
                                u_sum = tau.u;
                                _flush_fragment(v_tmp_tcs[i], v_tmp, u_sum, idx,
                                                v_new_tcs);
                                continue;
                        }

                        u_sum += tau.u;
                        v_tmp.push_back(tau);
                        ret = wcrt(v_tmp);

                        /* constraint 3.4 (schedulable) and 3.3 (size <= sigma) */
                        if (v_tmp.size() > 1 &&
                                        (ret == SCHED_FAILED || u_sum > limit)) {
                                u_sum -= tau.u;
                                v_tmp.pop_back();
                                _flush_fragment(v_tmp_tcs[i], v_tmp, u_sum, idx,
                                                v_new_tcs);
                                j--;
                                continue;
                        }
                }
                /* add last fragment */
                _flush_fragment(v_tmp_tcs[i], v_tmp, u_sum, idx, v_new_tcs);

                if (v_new_tcs.size() > frag_start)
                        ctx.k += v_new_tcs.size() - frag_start - 1;
        }
        v_tcs.clear();
        v_tcs = v_new_tcs;
        sort_dec_tc_size(v_tcs);
        for (unsigned int i = 0; i < v_tcs.size(); i++)
                v_tcs[i].gcd = gcd(v_tcs[i].v_tasks);

        printf("Initial Number of TC:   %d\n", ctx.prm.m);
        printf("Current Number of TC:   %ld\n", v_tcs.size());
        printf("Number of Cuts:         %d\n", ctx.k);
        printf("Number of CutsMax:      %d\n", ctx.k_max);
}

void input_prm(int argc, char **argv, struct params &prm)
{
        if (argc != 2) {
                printf("Wrong Number of Arguments!\n");
                exit(0);
        }
        prm.s = atoi(argv[1]);
        verify_prm(prm);
}

void init_ctx(vector<struct tc> &v_tcs, struct params &prm, 
                struct context &ctx)
{
        float part_time;

        ctx.prm = prm;
        ctx.cores_count = 0;
        ctx.tasks_count = 0;
        ctx.sched_ok_count = 0;
        ctx.sched_failed_count = 0;
        ctx.pcu_cores_count = 0;
        ctx.zcu_cores_count = 0;
        ctx.tcs_count = ctx.prm.m;

        part_time = ctx.p.part_time;
        ctx.p = {0};
        ctx.p.part_time = part_time;
}
