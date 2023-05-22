#include "generator.h"
#include "sched_analysis.h"

/* harmonic chains table TODO should be based on WATERS challenge model */
static int harm[4][5] = {
        {4000, 8000, 16000, 32000, 64000},
        {5000, 10000, 25000, 50000, 100000},
        {5000, 15000, 30000, 60000, 120000},
        {10000, 20000, 40000, 80000, 160000},
};

static void _assign_id(vector<struct item> &v_itms)
{
        for (unsigned int i = 0; i < v_itms.size(); i++)
                v_itms[i].id = i;
}

static void _check_params(struct params &prm)
{
        if (prm.a != BFDU_F && prm.a != WFDU_F) {
                printf("Invalid params: prm.a rule -> BFDU_F = 1 || WFDU = 2\n\n");
                exit(0);
        }

        if (prm.n < MINN || prm.n > MAXN) {
                printf("Invalid params: prm.n rule -> [10 <= n <= 10000]\n\n");
                exit(0);
        }

        if (prm.phi < MINPHI || prm.phi > MAXPHI) {
                printf("Invalid params: prm.phi rule -> [%d <= prm.phi <= %d]\n\n", 
                                MINPHI, MAXPHI);
                exit(0);
        }
}

static void _gen_harmonic_task(struct task &tau, struct params &prm, int i, int x)
{
        int y;
        int real_t;
        float real_c;
        float real_u;
        float udiff;

        while (1) {
                y  = gen_rand(0, 4);
                real_t = harm[x][y];
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

static int _gen_tc_set(vector<struct item> &v_itms, struct params &prm,
                struct context &ctx)
{
        int x;
        int ncount;
        int task_nbr;
        struct item itm;
        struct task tau;

        ncount = 0;

        printf("\n\n");
        printf("+=====================================+\n");
        printf("| INSTANCE GENERATION                 |\n");
        printf("+=====================================+\n");

        _check_params(prm);

        /* generate task-chains set */
        while (ncount != prm.n) {
                itm = {0};
                itm.id = ncount;
                itm.idx = 0;
                itm.size = 0;
                task_nbr = gen_rand(MINTASKNBR, MAXTASKNBR);
                itm.memcost = gen_rand(MINMEMCOST, MAXMEMCOST);
                itm.disp_count = 0;
                itm.swap_count = 0;
                itm.is_let = NO;
                itm.is_allocated = NO;
                x = gen_rand(0, 3);

                for (int i = 0; i < task_nbr; i++) {
                        tau = {0};
                        tau.is_let = NO;
                        tau.tc_id = itm.id;

                        _gen_harmonic_task(tau, prm, i, x);

                        itm.v_tasks.push_back(tau);
                        itm.size += tau.u;
                }

                v_itms.push_back(itm);
                v_itms[ncount].size = itm.size;
                ncount++;
                printf("%d\n", ncount);
        }
        printf("\n");

        sort_dec_itm_size(v_itms);
        _assign_id(v_itms);

        for (unsigned int i = 0; i < v_itms.size(); i++)
                v_itms[i].gcd = compute_gcd(v_itms[i].v_tasks);

        return 0;
}

static void _partitioning(vector<struct item> &v_itms, struct context &ctx)
{
        int idx;
        int u_sum;
        int uniq_id;
        struct item itm;
        vector<struct task> v_tmp;
        vector<struct item> v_new_itms;
        vector<struct item> v_tmp_itms;

        idx = 0;
        u_sum = 0;
        uniq_id = 1;
        ctx.cuts_count = 0;
        ctx.frags_count = 0;

        /* store itms > phi */
        for (unsigned int i = 0; i < v_itms.size(); i++) {
                if (v_itms[i].size >= ctx.prm.phi - EPSILON)
                        v_tmp_itms.push_back(v_itms[i]);
                else
                        v_new_itms.push_back(v_itms[i]);
        }

        /* fragmentation */
        for (unsigned int i = 0; i < v_tmp_itms.size(); i++) {
                idx = 0;
                for (unsigned int j = 0; j < v_tmp_itms[i].v_tasks.size(); j++) {
                        u_sum += v_tmp_itms[i].v_tasks[j].u;
                        v_tmp_itms[i].v_tasks[j].tc_id = v_tmp_itms[i].id;
                        v_tmp.push_back(v_tmp_itms[i].v_tasks[j]);
                        /* add fragment */
                        if (u_sum >= ctx.prm.phi - EPSILON) {
                                u_sum -= v_tmp_itms[i].v_tasks[j].u;
                                v_tmp.pop_back();
                                itm = {0};
                                itm.id = v_tmp_itms[i].id;
                                itm.idx = idx;
                                itm.size = u_sum;
                                itm.memcost = v_tmp_itms[i].memcost;
                                itm.disp_count = 0;
                                itm.swap_count = 0;
                                itm.is_let = NO;
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
                                itm.idx = idx;
                                itm.size = u_sum;
                                itm.memcost = v_tmp_itms[i].memcost;
                                itm.disp_count = 0;
                                itm.swap_count = 0;
                                itm.is_let = NO;
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

void input(int argc, char **argv, struct params &prm)
{
        if (argc != 4) {
                printf("Wrong Number of Arguments!\n");
                exit(0);
        }
        prm.n = atoi(argv[1]);
        prm.phi = atoi(argv[2]);
        prm.a = atoi(argv[3]);
        _check_params(prm);
}

void init_ctx(vector<struct item> &v_itms, struct params &prm, struct context &ctx)
{
        float frag_time;

        ctx.prm = prm;
        ctx.cycl_count = 0;
        ctx.bins_count = 0;
        ctx.alloc_count = 0;
        ctx.tasks_count = 0;
        ctx.sched_ok_count = 0;
        ctx.sched_failed_count = 0;
        ctx.itms_size = 0;
        ctx.itms_nbr = ctx.prm.n;
        ctx.itms_count = ctx.prm.n;

        frag_time = ctx.p.frag_time;
        ctx.p = {0};
        ctx.p.frag_time = frag_time;

        for (unsigned int i = 0; i < v_itms.size(); i++) 
                ctx.itms_size += v_itms[i].size;

        ctx.bins_min = abs(ctx.itms_size / ctx.prm.phi) + 1;

        printf("Minimum Number of Cores Required: %u\n", ctx.bins_min);
        printf("Total Utilization of Task-Chains: %u\n\n", ctx.itms_size);
}

void gen_tc_set(vector<struct item> &v_itms, struct params &prm,
                struct context &ctx)
{
        int ret; 

        ret = 0;
        ctx.prm = prm;

        while (1) {
                ret = _gen_tc_set(v_itms, prm, ctx);
                if (ret == -1) {
                        printf("ERR! data set generation\n");
                        v_itms.clear();
                        continue;
                } else
                        return;
        }
}

void partitioning(vector<struct item> &v_itms, struct context &ctx)
{
        _partitioning(v_itms, ctx);
}
