#include "generator.h"
#include "sched_analysis.h"

#define MINMAXTU 10
#define MAXC     100

#define MINN     5
#define MAXN     10000

static int gen_rand(int min, int max) 
{
        random_device rd;
        mt19937 gen(rd());
        uniform_int_distribution<> distr(min, max);

        return distr(gen);
}

static int cmp_dec(const struct item &a, const struct item &b)
{
        return a.size > b.size;
}

static void sort_dec(vector<struct item> &v_itms)
{
        sort(v_itms.begin(), v_itms.end(), cmp_dec);
}

static void assign_id(vector<struct item> &v_itms)
{
        for (unsigned int i = 0; i < v_itms.size(); i++)
                v_itms[i].id = i;
}

static void check_params(struct params &prm)
{
        if (prm.n < MINN || prm.n > MAXN) {
                printf("Invalid params: prm.n rule -> [10 <= n <= 10000]\n\n");
                exit(0);
        }

        if (prm.c < prm.phi || prm.c > MAXC) {
                printf("Invalid params: prm.c rule -> [prm.phi <= c <= 100]\n\n");
                exit(0);
        }

        if (prm.phi > MAXC) {
                printf("Invalid params: prm.phi rule -> [prm.phi < 90]\n\n");
                exit(0);
        }

        if (prm.max_tu < MINMAXTU || prm.max_tu > prm.phi) {
                printf("Invalid params: prm.max_tu rule -> [10 <= prm.max_tu < prm.phi]\n\n");
                exit(0);
        }
}

static int _gen_tc_set(vector<struct item> &v_itms, struct params &prm,
                struct context &ctx)
{
        int task_nbr;
        int rand;
        int ncount = 0;
        int lf_size = 0;
        int rf_size = 0;

        printf("\n\n");
        printf("+=====================================+\n");
        printf("| INSTANCE GENERATION                 |\n");
        printf("+=====================================+\n");

        check_params(prm);

        /* generate task-chains set */
        while (ncount != prm.n) {
                struct item itm;
                itm.id = ncount;
                itm.tc.u = 0;
                task_nbr = gen_rand(MINTASKNBR, MAXTASKNBR);
                itm.nbr_cut = task_nbr - 1;
                itm.is_frag = NO;
                itm.is_fragmented = NO;
                itm.is_allocated = NO;

                for (int i = 0; i < task_nbr; i++) {
                        struct task tau;
                        tau.u = gen_rand(1, prm.max_tu);
                        tau.c = gen_rand(MINWCET, MAXWCET);
                        tau.t = ceilf(((float)tau.c/(float)tau.u) * PERCENT);
                        tau.d = tau.t; /* implicit deadline */
                        tau.r = 0;
                        tau.p = i + 1; /* 1 is highest priority */
                        tau.id = i;

                        itm.tc.v_tasks.push_back(tau);
                        itm.tc.u += tau.u;
                }

                if (itm.tc.u > prm.c)
                        continue;

                if (wcrt(itm.tc.v_tasks) == SCHED_FAILED)
                        continue;

                v_itms.push_back(itm);
                v_itms[ncount].size = itm.tc.u;
                ncount++;
        }
        printf("\n");

        /* generate cuts for each chain */
        for (unsigned int i = 0; i < v_itms.size(); i++) {
                /* iterate over tasks */
                unsigned int count = 0;
                int cut_id = 0;
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

                        rand = gen_rand(NO, YES);
                        if (rand == YES && v_itms[i].size <= prm.phi)
                                continue;

                        /* sometimes do not add the cut, only for size < phi */
                        if (lf_size > prm.phi || rf_size > prm.phi) {
                                continue;
                        } else {
                                v_itms[i].tc.v_cuts.push_back(c);
                                cut_id++;
                        }
                }
                lf_size = 0;
                rf_size = 0;
        }
        sort_dec(v_itms);
        assign_id(v_itms);

        return 0;
}

void init_ctx(vector<struct item> &v_itms, struct params &prm, struct context &ctx)
{
        ctx.prm = prm;
        ctx.redu_time = 0.0;
        ctx.alloc_time = 0.0;
        ctx.e_time = 0.0;
        ctx.wca_time = 0.0;
        ctx.opti_time = 0.0;
        ctx.standard_dev = 0.0;
        ctx.opti_bins = 0.0;
        ctx.cycl_count = 0;
        ctx.bins_count = 0;
        ctx.alloc_count = 0;
        ctx.frags_count = 0;
        ctx.cuts_count = 0;
        ctx.tasks_count = 0;
        ctx.sched_ok_count = 0;
        ctx.sched_failed_count = 0;
        ctx.sched_imp = 0;
        ctx.sched_rate_bef = 0.0;
        ctx.sched_rate_aft = 0.0;
        ctx.itms_size = 0;
        ctx.itms_nbr = ctx.prm.n;
        ctx.itms_count = ctx.prm.n - 1;

        for (int i = 0; i < ctx.prm.n; i++) 
                ctx.itms_size += v_itms[i].size;

        ctx.bins_min = abs(ctx.itms_size / ctx.prm.phi) + 1;

        printf("Total Utilization of Task-Chains: %u\n", ctx.itms_size);
        printf("Minimum Number of Cores Required: %u\n", ctx.bins_min);
}

void gen_tc_set(vector<struct item> &v_itms, struct params &prm,
                struct context &ctx)
{
        while (1) {
                int ret = -1;

                vector<struct item> v_itms_tmp;
                ret = _gen_tc_set(v_itms_tmp, prm, ctx);

                if (ret == 0) {
                        v_itms = v_itms_tmp;
                        break;
                }
        }
}
