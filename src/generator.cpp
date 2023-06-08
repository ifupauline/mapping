#include "generator.h"
#include "sched_analysis.h"

/* WATERS2015 task periods */
static int chain[1][8] = {
        {1000, 2000, 5000, 10000, 20000, 50000, 100000, 200000},
};

static void _assign_id(vector<struct item> &v_itms)
{
        for (unsigned int i = 0; i < v_itms.size(); i++)
                v_itms[i].id = i;
}

static void _check_params_normal(struct params &prm)
{
        if (prm.e < MAXMAXTU || prm.e > PHI) {
                printf("Invalid params: prm.e rule -> [%d <= e <= %d]\n\n", 
                                MAXMAXTU + 1,  PHI);
                exit(0);
        }

        if (prm.n < MINN || prm.n > MAXN) {
                printf("Invalid params: prm.n rule -> [10 <= n <= 10000]\n\n");
                exit(0);
        }
}

static void _check_params_eden(struct params &prm)
{
        if (prm.e < MAXMAXTU || prm.e > PHI) {
                printf("Invalid params: prm.e rule -> [%d <= e <= %d]\n\n", 
                                MAXMAXTU + 1,  PHI);
                exit(0);
        }
}

static int _compute_colors(vector<struct item> &v_itms, struct context &ctx)
{
        /* count color */
        int red;
        int blue;
        int yellow;
        int green;
        int cyan;
        int purple;
        int white;

        red = 0;
        blue = 0;
        yellow = 0;
        green = 0;
        cyan = 0;
        purple = 0;
        white = 0;

        ctx.cs = {0};
        ctx.itms_size = 0;

        for (unsigned int i = 0; i < v_itms.size(); i++) {
                ctx.itms_size += v_itms[i].size;
                if (v_itms[i].color == RED) {
                        red++;
                        ctx.cs.red += v_itms[i].size;
                } else if (v_itms[i].color == BLUE) {
                        blue++;
                        ctx.cs.blue += v_itms[i].size;
                } else if (v_itms[i].color == YELLOW) {
                        yellow++;
                        ctx.cs.yellow += v_itms[i].size;
                } else if (v_itms[i].color == GREEN) {
                        green++;
                        ctx.cs.green += v_itms[i].size;
                } else if (v_itms[i].color == CYAN) {
                        cyan++;
                        ctx.cs.cyan += v_itms[i].size;
                } else if (v_itms[i].color == PURPLE) {
                        purple++;
                        ctx.cs.purple += v_itms[i].size;
                } else {
                        white++;
                        ctx.cs.white += v_itms[i].size;
                }
        }

        if (red == 0 || blue == 0 || yellow == 0 || green == 0 || cyan == 0 || 
                        purple == 0 || white == 0)
                return -1;

        ctx.bins_min = abs(ctx.itms_size / PHI) + 1;
        ctx.cs.red_bins_min = abs(ctx.cs.red / PHI) + 1;
        ctx.cs.blue_bins_min = abs(ctx.cs.blue / PHI) + 1;
        ctx.cs.yellow_bins_min = abs(ctx.cs.yellow / PHI) + 1;
        ctx.cs.green_bins_min = abs(ctx.cs.green / PHI) + 1;
        ctx.cs.cyan_bins_min = abs(ctx.cs.cyan / PHI) + 1;
        ctx.cs.purple_bins_min = abs(ctx.cs.purple / PHI) + 1;
        ctx.cs.white_bins_min = abs(ctx.cs.white / PHI) + 1;

        printf("Number of TC RED:    %d size: %d\n", red, ctx.cs.red);
        printf("Number of TC BLUE:   %d size: %d\n", blue, ctx.cs.blue);
        printf("Number of TC YELLOW: %d size: %d\n", yellow, ctx.cs.yellow);
        printf("Number of TC GREEN:  %d size: %d\n", green, ctx.cs.green);
        printf("Number of TC CYAN:   %d size: %d\n", cyan, ctx.cs.cyan);
        printf("Number of TC PURPLE: %d size: %d\n", purple, ctx.cs.purple);
        printf("Number of TC WHITE:  %d size: %d\n", white, ctx.cs.white);
        printf("------------------------------------------------------\n");
        printf("Minimum Number of RED    Cores: %u\n", ctx.cs.red_bins_min);
        printf("Minimum Number of BLUE   Cores: %u\n", ctx.cs.blue_bins_min);
        printf("Minimum Number of YELLOW Cores: %u\n", ctx.cs.yellow_bins_min);
        printf("Minimum Number of GREEN  Cores: %u\n", ctx.cs.green_bins_min);
        printf("Minimum Number of CYAN   Cores: %u\n", ctx.cs.cyan_bins_min);
        printf("Minimum Number of PURPLE Cores: %u\n", ctx.cs.purple_bins_min);
        printf("Minimum Number of WHITE  Cores: %u\n", ctx.cs.white_bins_min);
        printf("------------------------------------------------------\n");
        printf("Total Number of Cores    %u\n", ctx.bins_min);
        printf("Total Utilization of TC: %u\n\n", ctx.itms_size);
        printf("------------------------------------------------------\n");

        return 0;
}

/* WATERS 2019 CRTX A57 2000 MhZ */
static void _create_waters2019(struct item &itm)
{
        struct item waters2019;
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

        /* can_polling */
        can_polling = {0};
        can_polling.c = 499;
        can_polling.t = 10000;
        can_polling.d = 10000;
        can_polling.r = 0;
        can_polling.id = 0;
        can_polling.tc_id = itm.id;
        can_polling.is_let = NO;
        can_polling.u = ceil(((float)can_polling.c/(float)can_polling.t) * PERMILL);

        /* lidar */
        lidar = {0};
        lidar.c = 11759;
        lidar.t = 33000;
        lidar.d = 33000;
        lidar.r = 0;
        lidar.id = 1;
        lidar.tc_id = itm.id;
        lidar.is_let = NO;
        lidar.u = ceil(((float)lidar.c/(float)lidar.t) * PERMILL);

        /* cam_grabber */
        cam_grabber = {0};
        cam_grabber.c = 14986;
        cam_grabber.t = 33000;
        cam_grabber.d = 33000;
        cam_grabber.r = 0;
        cam_grabber.id = 2;
        cam_grabber.tc_id = itm.id;
        cam_grabber.is_let = NO;
        cam_grabber.u = ceil(((float)cam_grabber.c/(float)cam_grabber.t) * PERMILL);

        /* localization */
        localization = {0};
        localization.c = 142375;
        localization.t = 400000;
        localization.d = 400000;
        localization.r = 0;
        localization.id = 3;
        localization.tc_id = itm.id;
        localization.is_let = NO;
        localization.u = ceil(((float)localization.c/(float)localization.t) * PERMILL);

        /* detection */
        detection = {0};
        detection.c = 124956;
        detection.t = 200000;
        detection.d = 200000;
        detection.r = 0;
        detection.id = 4;
        detection.tc_id = itm.id;
        detection.is_let = NO;
        detection.u = ceil(((float)detection.c/(float)detection.t) * PERMILL);

        /* lane detection */
        lane_detection = {0};
        lane_detection.c = 36416;
        lane_detection.t = 66000;
        lane_detection.d = 66000;
        lane_detection.r = 0;
        lane_detection.id = 5;
        lane_detection.tc_id = itm.id;
        lane_detection.is_let = NO;
        lane_detection.u = ceil(((float)lane_detection.c/(float)lane_detection.t) * PERMILL);

        /* ekf */
        ekf = {0};
        ekf.c = 4399;
        ekf.t = 15000;
        ekf.d = 15000;
        ekf.r = 0;
        ekf.id = 6;
        ekf.tc_id = itm.id;
        ekf.is_let = NO;
        ekf.u = ceil(((float)ekf.c/(float)ekf.t) * PERMILL);

        /* planner */
        planner = {0};
        planner.c = 11371;
        planner.t = 15000;
        planner.d = 15000;
        planner.r = 0;
        planner.id = 7;
        planner.tc_id = itm.id;
        planner.is_let = NO;
        planner.u = ceil(((float)planner.c/(float)planner.t) * PERMILL);

        /* control */
        control = {0};
        control.c = 1609;
        control.t = 5000;
        control.d = 5000;
        control.r = 0;
        control.id = 8;
        control.tc_id = itm.id;
        control.is_let = NO;
        control.u = ceil(((float)control.c/(float)control.t) * PERMILL);

        /* can write */
        can_write = {0};
        can_write.c = 499;
        can_write.t = 10000;
        can_write.d = 10000;
        can_write.r = 0;
        can_write.id = 9;
        can_write.tc_id = itm.id;
        can_write.is_let = NO;
        can_write.u = ceil(((float)can_write.c/(float)can_write.t) * PERMILL);

        /* tc waters 2019 */
        waters2019 = {0};
        waters2019.id = 0;
        waters2019.tc_idx = 0;
        waters2019.memcost = MINMEMCOST;
        waters2019.disp_count = 0;
        waters2019.swap_count = 0;
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

        itm = waters2019;
}

static void _create_task(struct task &tau, int i, int x)
{
        int y;
        int real_t;
        float udiff;
        float real_c;
        float real_u;

        while (1) {
                y  = gen_rand(0, 7);
                real_t = chain[x][y];
                real_c = gen_rand(MINWCET, MAXWCET);
                real_u = (real_c/real_t) * PERMILL;

                if (real_u > MAXMAXTU || real_u < MINMAXTU)
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

                tau.d = tau.t; /* implicit deadline */
                tau.r = 0;
                tau.p = i + 1; /* 1 is highest priority */
                tau.id = i;
                break;
        }
}

static void _create_tc(struct item &itm, int color, int minu, int maxu)
{
        int x;
        int task_nbr;
        struct task tau;

        while (1) {
                x = 0;
                itm = {0};
                itm.tc_idx = 0;
                itm.size = 0;
                task_nbr = gen_rand(MINTASKNBR, MAXTASKNBR);
                itm.memcost = MINMEMCOST;
                itm.e2ed = 0;
                itm.color = color;
                itm.is_frag = NO;
                itm.disp_count = 0;
                itm.swap_count = 0;
                itm.is_let = NO;
                itm.is_allocated = NO;

                for (int i = 0; i < task_nbr; i++) {
                        tau = {0};
                        tau.is_let = NO;
                        tau.tc_id = itm.id;

                        _create_task(tau, i, x);

                        itm.v_tasks.push_back(tau);
                        itm.size += tau.u;
                }

                if (itm.size < minu || itm.size > maxu)
                        continue;
                else
                        return;
        }
}

static int _gen_eden_set(vector<struct item> &v_itms, struct params &prm, 
                struct context &ctx)
{
        printf("\n\n");
        printf("+=====================================+\n");
        printf("| EDEN INSTANCE GENERATION            |\n");
        printf("+=====================================+\n");

        struct item itm;

        /* red -> waters */
        itm = {0};
        _create_waters2019(itm);
        v_itms.push_back(itm);

        /* blue */
        itm = {0};
        _create_tc(itm, BLUE, 500, C);
        v_itms.push_back(itm);

        /* yellow */
        itm = {0};
        _create_tc(itm, YELLOW, 500, C);
        v_itms.push_back(itm);

        /* green */
        itm = {0};
        _create_tc(itm, GREEN, 500, C);
        v_itms.push_back(itm);

        /* cyan */
        itm = {0};
        _create_tc(itm, CYAN, 500, C);
        v_itms.push_back(itm);

        /* purple */
        itm = {0};
        _create_tc(itm, PURPLE, 500, C);
        v_itms.push_back(itm);

        /* white */
        for (int i = 0; i < 4; i++) {
                itm = {0};
                _create_tc(itm, WHITE, C, C*2);
                v_itms.push_back(itm);
        }
        sort_dec_itm_size(v_itms);
        _assign_id(v_itms);

        ctx.prm.n = v_itms.size();
        prm.n = v_itms.size();

        for (unsigned int i = 0; i < v_itms.size(); i++)
                v_itms[i].gcd = compute_gcd(v_itms[i].v_tasks);

        _compute_colors(v_itms, ctx);

        return 0;
}

static int _gen_normal_set(vector<struct item> &v_itms, struct params &prm,
                struct context &ctx)
{
        int x;
        int ret;
        int ncount;
        int task_nbr;
        struct item itm;
        struct task tau;

        ret = 0;
        itm = {0};
        ncount = 0;

        _check_params_normal(prm);

        printf("\n\n");
        printf("+=====================================+\n");
        printf("| NORMAL INSTANCE GENERATION          |\n");
        printf("+=====================================+\n");

        /* derive synthetic set from waters */
        while (ncount != prm.n) {
                itm = {0};
                itm.tc_idx = 0;
                itm.size = 0;
                task_nbr = gen_rand(MINTASKNBR, MAXTASKNBR);
                itm.memcost = MINMEMCOST;
                itm.e2ed = 0;
                itm.color = gen_rand(0,10);
                itm.is_frag = NO;
                itm.disp_count = 0;
                itm.swap_count = 0;
                itm.is_let = NO;
                itm.is_allocated = NO;
                x = 0;

                for (int i = 0; i < task_nbr; i++) {
                        tau = {0};
                        tau.is_let = NO;
                        tau.tc_id = itm.id;

                        _create_task(tau, i, x);

                        itm.v_tasks.push_back(tau);
                        itm.size += tau.u;
                }

                if (itm.size < C)
                        continue;

                v_itms.push_back(itm);
                ncount++;
                printf("%d\n", ncount);
        }
        printf("\n");

        sort_dec_itm_size(v_itms);
        _assign_id(v_itms);

        for (unsigned int i = 0; i < v_itms.size(); i++)
                v_itms[i].gcd = compute_gcd(v_itms[i].v_tasks);

        ret = _compute_colors(v_itms, ctx);

        if (ret == -1)
                return -1;

        return 0;
}

void partitioning(vector<struct item> &v_itms, struct context &ctx)
{
        int idx;
        int ret;
        int u_sum;
        int uniq_id;
        struct item itm;
        vector<struct task> v_tmp;
        vector<struct item> v_new_itms;
        vector<struct item> v_tmp_itms;

        idx = 0;
        ret = -1;
        u_sum = 0;
        uniq_id = 1;
        ctx.cuts_count = 0;
        ctx.frags_count = 0;

        /* store itms > phi */
        for (unsigned int i = 0; i < v_itms.size(); i++)
                v_tmp_itms.push_back(v_itms[i]);

        /* fragmentation */
        for (unsigned int i = 0; i < v_tmp_itms.size(); i++) {
                idx = 0;
                for (unsigned int j = 0; j < v_tmp_itms[i].v_tasks.size(); j++) {
                        ret = -1;
                        u_sum += v_tmp_itms[i].v_tasks[j].u;
                        v_tmp_itms[i].v_tasks[j].tc_id = v_tmp_itms[i].id;
                        v_tmp.push_back(v_tmp_itms[i].v_tasks[j]);
                        ret = wcrt(v_tmp);
                        if (ret == SCHED_FAILED)  {
                                u_sum -= v_tmp_itms[i].v_tasks[j].u;
                                v_tmp.pop_back();
                                itm = {0};
                                itm.id = v_tmp_itms[i].id;
                                itm.tc_idx = idx;
                                itm.size = u_sum;
                                itm.memcost = v_tmp_itms[i].memcost;
                                itm.e2ed = 0;
                                itm.disp_count = 0;
                                itm.swap_count = 0;
                                itm.color = v_tmp_itms[i].color;
                                itm.is_let = NO;
                                itm.is_frag = YES;
                                itm.is_allocated = NO;
                                itm.v_tasks = v_tmp;
                                v_new_itms.push_back(itm);
                                j--;
                                idx++;
                                ctx.cuts_count++;
                                u_sum = 0;
                                v_tmp.clear();
                        }

                        if (i == 0 && u_sum >= PHI) {
                                u_sum -= v_tmp_itms[i].v_tasks[j].u;
                                v_tmp.pop_back();
                                itm = {0};
                                itm.id = v_tmp_itms[i].id;
                                itm.tc_idx = idx;
                                itm.size = u_sum;
                                itm.memcost = v_tmp_itms[i].memcost;
                                itm.e2ed = 0;
                                itm.disp_count = 0;
                                itm.swap_count = 0;
                                itm.color = v_tmp_itms[i].color;
                                itm.is_let = NO;
                                itm.is_frag = YES;
                                itm.is_allocated = NO;
                                itm.v_tasks = v_tmp;
                                v_new_itms.push_back(itm);
                                j--;
                                idx++;
                                ctx.cuts_count++;
                                u_sum = 0;
                                v_tmp.clear();
                        }
                        if (i > 0 && u_sum >= ctx.prm.e) {
                                u_sum -= v_tmp_itms[i].v_tasks[j].u;
                                v_tmp.pop_back();
                                itm = {0};
                                itm.id = v_tmp_itms[i].id;
                                itm.tc_idx = idx;
                                itm.size = u_sum;
                                itm.memcost = v_tmp_itms[i].memcost;
                                itm.e2ed = 0;
                                itm.disp_count = 0;
                                itm.swap_count = 0;
                                itm.color = v_tmp_itms[i].color;
                                itm.is_let = NO;
                                itm.is_frag = YES;
                                itm.is_allocated = NO;
                                itm.v_tasks = v_tmp;
                                v_new_itms.push_back(itm);
                                j--;
                                idx++;
                                ctx.cuts_count++;
                                u_sum = 0;
                                v_tmp.clear();
                        }
                        /* add last fragment */
                        if (j == v_tmp_itms[i].v_tasks.size() - 1) {
                                itm = {0};
                                itm.id = v_tmp_itms[i].id;
                                itm.tc_idx = idx;
                                itm.size = u_sum;
                                itm.memcost = v_tmp_itms[i].memcost;
                                itm.e2ed = 0;
                                itm.disp_count = 0;
                                itm.swap_count = 0;
                                itm.color = v_tmp_itms[i].color;
                                itm.is_let = NO;
                                itm.is_frag = YES;
                                itm.is_allocated = NO;
                                itm.v_tasks = v_tmp;
                                v_new_itms.push_back(itm);
                                ctx.cuts_count++;
                                u_sum = 0;
                                v_tmp.clear();
                                ctx.frags_count++;
                        }
                }
        }
        v_itms.clear();
        v_itms = v_new_itms;
        sort_dec_itm_size(v_itms);
        for (unsigned int i = 0; i < v_itms.size(); i++)
                v_itms[i].gcd = compute_gcd(v_itms[i].v_tasks);

        for (unsigned int i = 0; i < v_itms.size(); i++) {
                for (unsigned int j = 0; j < v_itms[i].v_tasks.size(); j++) {
                        v_itms[i].v_tasks[j].uniq_id = uniq_id;
                        uniq_id++;
                }
        }
        printf("Initial Number of TC:   %d\n", ctx.prm.n);
        printf("Current Number of TC:   %ld\n", v_itms.size());
        printf("Number of Cuts: %d\n", ctx.cuts_count);
        printf("Number of TC Cuts: %d\n", ctx.frags_count);
}

int gen_rand(int min, int max) 
{
        random_device rd;
        mt19937 gen(rd());
        uniform_int_distribution<> distr(min, max);

        return distr(gen);
}

void input_normal(int argc, char **argv, struct params &prm)
{
        if (argc != 3) {
                printf("Wrong Number of Arguments!\n");
                exit(0);
        }
        prm.n = atoi(argv[1]);
        prm.e = atoi(argv[2]);
        _check_params_normal(prm);
}

void input_eden(int argc, char **argv, struct params &prm)
{
        if (argc != 2) {
                printf("Wrong Number of Arguments!\n");
                exit(0);
        }
        prm.e = atoi(argv[1]);
        _check_params_eden(prm);
}

void init_ctx(vector<struct item> &v_itms, struct params &prm, 
                struct context &ctx)
{
        float frag_time;

        ctx.prm = prm;
        ctx.cycl_count = 0;
        ctx.bins_count = 0;
        ctx.alloc_count = 0;
        ctx.tasks_count = 0;
        ctx.intra_comm_count = 0;
        ctx.inter_comm_count = 0;
        ctx.e2e_ok_count = 0;
        ctx.e2e_failed_count = 0;
        ctx.sched_ok_count = 0;
        ctx.sched_failed_count = 0;
        ctx.itms_size = 0;
        ctx.itms_nbr = ctx.prm.n;
        ctx.itms_count = ctx.prm.n;

        frag_time = ctx.p.frag_time;
        ctx.p = {0};
        ctx.p.frag_time = frag_time;
}

void gen_normal_set(vector<struct item> &v_itms, struct params &prm,
                struct context &ctx)
{
        int ret; 

        ret = 0;
        ctx.prm = prm;

        while (1) {
                ret = _gen_normal_set(v_itms, prm, ctx);
                if (ret == -1) {
                        printf("ERR! data set generation\n");
                        v_itms.clear();
                        continue;
                } else
                        return;
        }
}

void gen_eden_set(vector<struct item> &v_itms, struct params &prm, 
                struct context &ctx)
{
        ctx.prm = prm;
        _gen_eden_set(v_itms, prm, ctx);
}
