#include "generator.h"
#include "sched_analysis.h"

static int counter = 0;

/* WATERS2015 task periods */
static int chain_static[1][6] = {
        {1000, 2000, 5000, 10000, 20000, 50000},
};

static int chain_dynamic[1][8] = {
        {1000, 2000, 5000, 10000, 20000, 50000, 100000, 200000},
};

/* WATERS 2019 */
static void _create_waters2019(struct tc &tc)
{
        struct tc waters2019;
        struct task can_polling;
        struct task lidar;
        struct task cam_grabber;
        struct task localization;
        struct task detection;
        struct task lane_detection;
        struct task ekf;
        struct task planner;
        struct task control;
        struct task can_write;

        waters2019 = {0};
        can_polling = {0};
        lidar = {0};
        cam_grabber = {0};
        localization = {0};
        detection = {0};
        lane_detection = {0};
        ekf = {0};
        planner = {0};
        control = {0};
        can_write = {0};

        /* can_polling */
        can_polling = {0};
        can_polling.c = 499;
        can_polling.t = 10000;
        can_polling.r = 0;
        can_polling.id = 0;
        can_polling.tc_id = tc.id;
        can_polling.is_let = NO;
        can_polling.u = ceil(((float)can_polling.c/(float)can_polling.t) * PERMILL);

        /* lidar */
        lidar = {0};
        lidar.c = 11759;
        lidar.t = 33000;
        lidar.r = 0;
        lidar.id = 1;
        lidar.tc_id = tc.id;
        lidar.is_let = NO;
        lidar.u = ceil(((float)lidar.c/(float)lidar.t) * PERMILL);

        /* cam_grabber */
        cam_grabber = {0};
        cam_grabber.c = 14986;
        cam_grabber.t = 33000;
        cam_grabber.r = 0;
        cam_grabber.id = 2;
        cam_grabber.tc_id = tc.id;
        cam_grabber.is_let = NO;
        cam_grabber.u = ceil(((float)cam_grabber.c/(float)cam_grabber.t) * PERMILL);

        /* localization */
        localization = {0};
        localization.c = 142375;
        localization.t = 400000;
        localization.r = 0;
        localization.id = 3;
        localization.tc_id = tc.id;
        localization.is_let = NO;
        localization.u = ceil(((float)localization.c/(float)localization.t) * PERMILL);

        /* detection */
        detection = {0};
        detection.c = 124956;
        detection.t = 200000;
        detection.r = 0;
        detection.id = 4;
        detection.tc_id = tc.id;
        detection.is_let = NO;
        detection.u = ceil(((float)detection.c/(float)detection.t) * PERMILL);

        /* lane detection */
        lane_detection = {0};
        lane_detection.c = 36416;
        lane_detection.t = 66000;
        lane_detection.r = 0;
        lane_detection.id = 5;
        lane_detection.tc_id = tc.id;
        lane_detection.is_let = NO;
        lane_detection.u = ceil(((float)lane_detection.c/(float)lane_detection.t) * PERMILL);

        /* ekf */
        ekf = {0};
        ekf.c = 4399;
        ekf.t = 15000;
        ekf.r = 0;
        ekf.id = 6;
        ekf.tc_id = tc.id;
        ekf.is_let = NO;
        ekf.u = ceil(((float)ekf.c/(float)ekf.t) * PERMILL);

        /* planner */
        planner = {0};
        planner.c = 11371;
        planner.t = 15000;
        planner.r = 0;
        planner.id = 7;
        planner.tc_id = tc.id;
        planner.is_let = NO;
        planner.u = ceil(((float)planner.c/(float)planner.t) * PERMILL);

        /* control */
        control = {0};
        control.c = 1609;
        control.t = 5000;
        control.r = 0;
        control.id = 8;
        control.tc_id = tc.id;
        control.is_let = NO;
        control.u = ceil(((float)control.c/(float)control.t) * PERMILL);

        /* can write */
        can_write = {0};
        can_write.c = 499;
        can_write.t = 10000;
        can_write.r = 0;
        can_write.id = 9;
        can_write.tc_id = tc.id;
        can_write.is_let = NO;
        can_write.u = ceil(((float)can_write.c/(float)can_write.t) * PERMILL);

        waters2019 = {0};
        waters2019.id = 0;
        waters2019.tc_idx = 0;
        waters2019.memcost = 3;
        waters2019.color = RED;
        waters2019.is_let = NO;
        waters2019.is_allocated = NO;

        waters2019.size = can_polling.u + lidar.u + cam_grabber.u + localization.u + 
                detection.u + lane_detection.u + ekf.u + planner.u + control.u + 
                can_write.u;

        waters2019.v_tasks.push_back(can_polling);
        waters2019.v_tasks.push_back(lidar);
        waters2019.v_tasks.push_back(cam_grabber);
        waters2019.v_tasks.push_back(localization);
        waters2019.v_tasks.push_back(detection);
        waters2019.v_tasks.push_back(lane_detection);
        waters2019.v_tasks.push_back(ekf);
        waters2019.v_tasks.push_back(planner);
        waters2019.v_tasks.push_back(control);
        waters2019.v_tasks.push_back(can_write);

        tc = waters2019;
}

static void _assign_id(vector<struct tc> &v_tcs)
{
        for (unsigned int i = 0; i < v_tcs.size(); i++)
                v_tcs[i].id = i;
}

static void _check_prm(struct params &prm)
{
        if (prm.s < 150 || prm.s > PHI) {
                printf("Invalid params: prm.s rule -> [%d <= s <= %d]\n\n", 
                                150,  PHI);
                exit(0);
        }
}

static int _cmp_colors(vector<struct tc> &v_tcs, struct context &ctx)
{
        /* count color */
        int red;
        int blue;
        int yellow;
        int green;
        int cyan;
        int purple;
        int white;

        float tmp;

        red = 0;
        blue = 0;
        yellow = 0;
        green = 0;
        cyan = 0;
        purple = 0;
        white = 0;

        tmp = 0.0;
        ctx.cs = {0};
        ctx.tcs_size = 0;

        for (unsigned int i = 0; i < v_tcs.size(); i++) {
                ctx.tcs_size += v_tcs[i].size;
                if (v_tcs[i].color == RED) {
                        red++;
                        ctx.cs.red += v_tcs[i].size;
                } else if (v_tcs[i].color == BLUE) {
                        blue++;
                        ctx.cs.blue += v_tcs[i].size;
                } else if (v_tcs[i].color == YELLOW) {
                        yellow++;
                        ctx.cs.yellow += v_tcs[i].size;
                } else if (v_tcs[i].color == GREEN) {
                        green++;
                        ctx.cs.green += v_tcs[i].size;
                } else if (v_tcs[i].color == CYAN) {
                        cyan++;
                        ctx.cs.cyan += v_tcs[i].size;
                } else if (v_tcs[i].color == PURPLE) {
                        purple++;
                        ctx.cs.purple += v_tcs[i].size;
                } else {
                        white++;
                        ctx.cs.white += v_tcs[i].size;
                }
        }

        if (red == 0 || blue == 0 || yellow == 0 || green == 0 || cyan == 0 || 
                        purple == 0 || white == 0)
                return -1;


        tmp = (float)(ctx.tcs_size / (float)PHI);

        if (abs(ctx.tcs_size/PHI) == tmp)
                ctx.cores_min = abs(ctx.tcs_size / PHI);
        else
                ctx.cores_min = abs(ctx.tcs_size / PHI) + 1;

        printf("Number of TC RED:    %d size: %d\n", red, ctx.cs.red);
        printf("Number of TC BLUE:   %d size: %d\n", blue, ctx.cs.blue);
        printf("Number of TC YELLOW: %d size: %d\n", yellow, ctx.cs.yellow);
        printf("Number of TC GREEN:  %d size: %d\n", green, ctx.cs.green);
        printf("Number of TC CYAN:   %d size: %d\n", cyan, ctx.cs.cyan);
        printf("Number of TC PURPLE: %d size: %d\n", purple, ctx.cs.purple);
        printf("Number of TC WHITE:  %d size: %d\n", white, ctx.cs.white);
        printf("------------------------------------------------------\n");
        printf("Total Number of Cores    %u\n", ctx.cores_min);
        printf("Total Utilization of TC: %u\n\n", ctx.tcs_size);
        printf("------------------------------------------------------\n");

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
                        real_t = chain_dynamic[0][y];
                } else {
                        y  = gen_rand(0, 5);
                        real_t = chain_static[0][y];
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
                tau.id = i;
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

                tc.memcost = gen_rand(1, 3);
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
                else {
                        counter++;
                        //printf("%d\n", counter);
                        return;
                }
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
        //_create_waters2019(tc);
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
        _assign_id(v_tcs);

        ctx.prm.n = v_tcs.size();
        prm.n = v_tcs.size();

        for (unsigned int i = 0; i < v_tcs.size(); i++)
                v_tcs[i].gcd = cmp_gcd(v_tcs[i].v_tasks);

        _cmp_colors(v_tcs, ctx);

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
                                tc.memcost = v_tmp_tcs[i].memcost;
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
                                tc.memcost = v_tmp_tcs[i].memcost;
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

                        if (i > 0 && u_sum > ctx.prm.s - 10) {
                                u_sum -= v_tmp_tcs[i].v_tasks[j].u;
                                v_tmp.pop_back();
                                tc = {0};
                                tc.id = v_tmp_tcs[i].id;
                                tc.tc_idx = idx;
                                tc.size = u_sum;
                                tc.memcost = v_tmp_tcs[i].memcost;
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
                                tc.memcost = v_tmp_tcs[i].memcost;
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

        //for (unsigned int i = 0; i < v_tcs.size(); i++) {
        //        for (unsigned int j = 0; j < v_tcs[i].v_tasks.size(); j++) {
        //                v_tcs[i].v_tasks[j].uniq_id = uniq_id;
        //                uniq_id++;
        //        }
        //}
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
        ctx.tcs_size = 0;
        ctx.tcs_nbr = ctx.prm.n;
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
