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
        //int rand;
        int task_nbr;
        int lf_size;
        int rf_size;
        int cut_id;
        int ncount;
        unsigned int count;

        ncount = 0;
        lf_size = 0;
        rf_size = 0;

        printf("\n\n");
        printf("+=====================================+\n");
        printf("| INSTANCE GENERATION                 |\n");
        printf("+=====================================+\n");

        _check_params(prm);

        /* generate task-chains set */
        while (ncount != prm.n) {
                struct item itm;
                itm.id = ncount;
                itm.size = 0;
                task_nbr = gen_rand(MINTASKNBR, MAXTASKNBR);
                itm.nbr_cut = task_nbr - 1;
                itm.frag_id = -1;
                itm.memcost = gen_rand(MINMEMCOST, MAXMEMCOST);
                itm.disp_count = 0;
                itm.swap_count = 0;
                itm.is_let = NO;
                itm.is_frag = NO;
                itm.is_fragmented = NO;
                itm.is_allocated = NO;
                x = gen_rand(0, 3);

                for (int i = 0; i < task_nbr; i++) {
                        struct task tau;
                        tau.is_let = NO;

                        _gen_harmonic_task(tau, prm, i, x);

                        itm.v_tasks.push_back(tau);
                        itm.size += tau.u;
                }
                if (itm.size > C)
                        continue;

                v_itms.push_back(itm);
                v_itms[ncount].size = itm.size;
                ncount++;
                printf("%d\n", ncount);
        }
        printf("\n");

        /* generate cuts for each chain */
        for (unsigned int i = 0; i < v_itms.size(); i++) {
                /* iterate over tasks */
                count = 0;
                cut_id = 0;
                for (unsigned int j = 0; j < v_itms[i].v_tasks.size() - 1; j++) {
                        struct cut c;
                        lf_size += v_itms[i].v_tasks[j].u;
                        rf_size = v_itms[i].size - lf_size;
                        c.id = cut_id;
                        c.c_pair.first = lf_size;
                        c.c_pair.second = rf_size;

                        if (lf_size > prm.phi || rf_size > prm.phi) {
                                count++;
                                if (count == v_itms[i].v_tasks.size() - 1)
                                        return -1;
                        }

                        /* copy tasks to left fragment */
                        for (unsigned int k = j; k >= 0; k--) {
                                c.v_tasks_lf.push_back(v_itms[i].v_tasks[k]);
                                /* for the first task */
                                if (k == 0)
                                        break;
                        }

                        /* copy tasks to right fragment */
                        for (unsigned int k = j + 1; k <= v_itms[i].v_tasks.size() - 1; k++)
                                c.v_tasks_rf.push_back(v_itms[i].v_tasks[k]);

                        v_itms[i].v_cuts.push_back(c);
                        cut_id++;
                }
                lf_size = 0;
                rf_size = 0;
        }
        sort_dec_itm_size(v_itms);
        _assign_id(v_itms);

        for (unsigned int i = 0; i < v_itms.size(); i++)
                v_itms[i].gcd = compute_gcd(v_itms[i].v_tasks);

        return 0;
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
        if (argc != 3) {
                printf("Wrong Number of Arguments!\n");
                exit(0);
        }
        prm.n = atoi(argv[1]);
        prm.phi = atoi(argv[2]);
        _check_params(prm);
}

void init_ctx(vector<struct item> &v_itms, struct params &prm, struct context &ctx)
{
        ctx.prm = prm;

        ctx.cycl_count = 0;
        ctx.bins_count = 0;
        ctx.alloc_count = 0;
        ctx.frags_count = 0;
        ctx.cuts_count = 0;
        ctx.tasks_count = 0;
        ctx.sched_ok_count = 0;
        ctx.sched_failed_count = 0;
        ctx.itms_size = 0;
        ctx.itms_nbr = ctx.prm.n;
        ctx.itms_count = ctx.prm.n;

        ctx.p = {0};

        for (int i = 0; i < ctx.prm.n; i++) 
                ctx.itms_size += v_itms[i].size;

        ctx.bins_min = abs(ctx.itms_size / ctx.prm.phi) + 1;

        printf("Total Utilization of Task-Chains: %u\n", ctx.itms_size);
        printf("Minimum Number of Cores Required: %u\n", ctx.bins_min);
}

void gen_tc_set(vector<struct item> &v_itms, struct params &prm,
                struct context &ctx)
{
        int ret; 

        ret = 0;

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
