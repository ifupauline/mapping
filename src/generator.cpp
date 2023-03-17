#include "generator.h"
#include "sched_analysis.h"

/* params */
#define PRECISION  0.40

#define MINN       5
#define MAXN       10000

#define MINPHI     C/2
#define MAXPHI     C

#define MINMAXTU   1
#define MAXMAXTU   20

#define MINWCET    1
#define MAXWCET    20

#define MINTASKNBR 2
#define MAXTASKNBR 20

/* harmonic chains table */
static int harm[4][5] = {
        {4, 8, 16, 32, 64},
        {5, 10, 25, 50, 100},
        {5, 15, 30, 60, 120},
        {10, 20, 40, 80, 160},
};

static int _gen_rand(int min, int max) 
{
        random_device rd;
        mt19937 gen(rd());
        uniform_int_distribution<> distr(min, max);

        return distr(gen);
}

static void _assign_id(vector<struct item> &v_itms)
{
        for (unsigned int i = 0; i < v_itms.size(); i++)
                v_itms[i].id = i;
}

void _check_params(struct params &prm)
{
        if (prm.n < MINN || prm.n > MAXN) {
                printf("Invalid params: prm.n rule -> [10 <= n <= 10000]\n\n");
                exit(0);
        }

        if (prm.phi < MINPHI || prm.phi > MAXPHI) {
                printf("Invalid params: prm.phi rule -> [prm.phi < %d]\n\n", 
                                MAXPHI);
                exit(0);
        }

        if (prm.h != NO && prm.h != YES) {
                printf("Invalid params: prm.h rule -> [%d -> NO || %d -> YES]\n\n", 
                                NO, YES);
                exit(0);
        }
}

static void _gen_non_harmonic_task(struct task &tau, struct params &prm, int i)
{
        tau.u = _gen_rand(MINMAXTU, MAXMAXTU);
        tau.c = _gen_rand(MINWCET, MAXWCET);
        tau.t = ceilf(((float)tau.c/(float)tau.u) * PERCENT);
        tau.d = tau.t; /* implicit deadline */
        tau.r = 0;
        tau.p = i + 1; /* 1 is highest priority */
        tau.id = i;
}

static void _gen_harmonic_task(struct task &tau, struct params &prm, int i, int x)
{
        int y;
        int real_t;
        float real_c;
        float real_u;
        float udiff;

        while (1) {
                y  = _gen_rand(0, 4);
                real_t = harm[x][y];
                real_c = _gen_rand(MINWCET, MAXWCET);
                real_u = (real_c/real_t) * PERCENT;

                /* c is minimum 1 */
                if (real_c < 1)
                        continue;

                if (real_u > MAXMAXTU || real_u < MINMAXTU)
                        continue;

                tau.u = ceil((real_c/real_t) * PERCENT);
                tau.c = ceil(real_c);
                tau.t = real_t;

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
        int ncount;
        int lf_size;
        int rf_size;
        int cut_id;
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
                itm.tc.u = 0;
                task_nbr = _gen_rand(MINTASKNBR, MAXTASKNBR);
                itm.nbr_cut = task_nbr - 1;
                itm.frag_id = -1;
                itm.disp_count = 0;
                itm.swap_count = 0;
                itm.is_frag = NO;
                itm.is_fragmented = NO;
                itm.is_allocated = NO;
                x = _gen_rand(0, 3);

                for (int i = 0; i < task_nbr; i++) {
                        struct task tau;

                        if (prm.h == NO)
                                _gen_non_harmonic_task(tau, prm, i);

                        else if (prm.h == YES)
                                _gen_harmonic_task(tau, prm, i, x);
                        else {
                                printf("ERR! input params!\n");
                                exit(0);
                        }
                        itm.tc.v_tasks.push_back(tau);
                        itm.tc.u += tau.u;
                }

                if (itm.tc.u > C)
                        continue;

                if (wcrt(itm.tc.v_tasks) == SCHED_FAILED)
                        continue;

                v_itms.push_back(itm);
                v_itms[ncount].size = itm.tc.u;
                ncount++;
                //printf("%d\n", ncount);
        }
        printf("\n");

        /* generate cuts for each chain */
        for (unsigned int i = 0; i < v_itms.size(); i++) {
                /* iterate over tasks */
                count = 0;
                cut_id = 0;
                for (unsigned int j = 0; j < v_itms[i].tc.v_tasks.size() - 1; j++) {
                        struct cut c;
                        lf_size += v_itms[i].tc.v_tasks[j].u;
                        rf_size = v_itms[i].tc.u - lf_size;
                        c.id = cut_id;
                        c.c_pair.first = lf_size;
                        c.c_pair.second = rf_size;

                        if (lf_size > prm.phi || rf_size > prm.phi) {
                                count++;
                                if (count == v_itms[i].tc.v_tasks.size() - 1)
                                        return -1;
                        } 

                        /* copy tasks to left fragment */
                        for (unsigned int k = j; k >= 0; k--) {
                                c.v_tasks_lf.push_back(v_itms[i].tc.v_tasks[k]);
                                /* for the first task */
                                if (k == 0)
                                        break;
                        }

                        /* copy tasks to right fragment */
                        for (unsigned int k = j + 1; k <= v_itms[i].tc.v_tasks.size() - 1; k++)
                                c.v_tasks_rf.push_back(v_itms[i].tc.v_tasks[k]);

                        /* if I want more constraints on nbr_cut per tc */
                        //rand = _gen_rand(NO, YES);
                        //if (rand == YES && v_itms[i].size <= prm.phi)
                        //        continue;

                        /* sometimes do not add the cut, only for size < phi */
                        if (lf_size > prm.phi || rf_size > prm.phi)
                                continue;

                        else {
                                v_itms[i].tc.v_cuts.push_back(c);
                                cut_id++;
                        }
                }
                lf_size = 0;
                rf_size = 0;
        }
        sort_dec_itm_size(v_itms);
        _assign_id(v_itms);

        return 0;
}

void input(int argc, char **argv, struct params &prm)
{
        if (argc != 4) {
                printf("Wrong Number of Arguments!\n");
                exit(0);
        }
        prm.n = atoi(argv[1]);
        prm.phi = atoi(argv[2]);
        prm.h = atoi(argv[3]);
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
        ctx.itms_count = ctx.prm.n - 1;

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
