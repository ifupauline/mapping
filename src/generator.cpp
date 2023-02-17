#include "generator.h"
#include "sched_analysis.h"

static int gen_rand(int min, int max) 
{
        random_device rd;
        mt19937 gen(rd());
        uniform_int_distribution<> distr(min, max);

        return distr(gen);
}

void init_ctx(struct params &prm, struct context &ctx)
{
        ctx.prm = prm;
        ctx.redu_time = 0.0;
        ctx.alloc_time = 0.0;
        ctx.frag_time = 0.0;
        ctx.e_time = 0.0;
        ctx.standard_dev = 0.0;
        ctx.opti_bins = 0.0;
        ctx.cycl_count = 0;
        ctx.bins_count = 0;
        ctx.alloc_count = 0;
        ctx.frags_count = 0;
        ctx.cuts_count = 0;
        ctx.itms_size = 0;
        ctx.itms_nbr = ctx.prm.n;
        ctx.itms_count = ctx.prm.n - 1;
}

void comp_min_bins(vector<struct item> &v_itms, struct context &ctx)
{
        for (int i = 0; i < ctx.prm.n; i++) 
                ctx.itms_size += v_itms[i].size;

        ctx.bins_min = abs(ctx.itms_size / ctx.prm.phi) + 1;

        printf("Number of Items: %d\n", ctx.itms_nbr);
        printf("Total Size of Items: %u\n", ctx.itms_size);
        printf("Minimum Number of Bins Required: %u\n", ctx.bins_min);
}

void gen_tc_set(vector<struct item> &v_itms, struct params &prm)
{
        int task_nbr;
        int ncount = 0;
        int phicount = 0;
        int lf_size = 0;
        int rf_size = 0;

        printf("\n\n");
        printf("+=====================================+\n");
        printf("| INSTANCE GENERATION                 |\n");
        printf("+=====================================+\n");

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
                        tau.u = gen_rand(1, prm.s);
                        tau.c = gen_rand(MINWCET, MAXWCET);
                        tau.t = ceilf(((float)tau.c/(float)tau.u) * PERCENT);
                        tau.d = tau.t; /* implicit deadline */
                        tau.r = 0;
                        tau.p = i + 1; /* 1 is highest priority */
                        tau.id = i;

                        itm.tc.v_tasks.push_back(tau);
                        itm.tc.u += tau.u;
                }

                if (wcrt(itm.tc.v_tasks) == SCHED_FAILED) {
                        continue;

                } else if (itm.tc.u <= prm.c && phicount != prm.fr && itm.tc.u > prm.phi) {
                        v_itms.push_back(itm);
                        v_itms[ncount].size = itm.tc.u;
                        ncount++;
                        phicount++;
                        continue;

                } else if (itm.tc.u <= prm.c && phicount == prm.fr) {
                        v_itms.push_back(itm);
                        v_itms[ncount].size = itm.tc.u;
                        ncount++;
                        continue;
                }
        }
        printf("\n");

        /* generate cuts for each chain */
        for (unsigned int i = 0; i < v_itms.size(); i++) {
                /* iterate over tasks */
                for (unsigned int j = 0; j < v_itms[i].tc.v_tasks.size() - 1; j++) {
                        struct cut c;
                        lf_size += v_itms[i].tc.v_tasks[j].u;
                        rf_size = v_itms[i].tc.u - lf_size;
                        c.id = j;
                        c.c_pair.first = lf_size;
                        c.c_pair.second = rf_size;

                        /* copy tasks to left fragment */
                        for (unsigned int k = j; k >= 0; k--) {
                                c.v_tasks_lf.push_back(v_itms[i].tc.v_tasks[k]);
                                /* for the first task */
                                if (k == 0)
                                        break;
                        }

                        /* copy tasks to right fragment */
                        for (unsigned int k = j + 1; k <= v_itms[i].tc.v_tasks.size() - 1; k++) {
                                c.v_tasks_rf.push_back(v_itms[i].tc.v_tasks[k]);
                        }
                        v_itms[i].tc.v_cuts.push_back(c);
                }
                lf_size = 0;
                rf_size = 0;
        }
}
