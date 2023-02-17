#include "generator.h"
#include "sched_analysis.h"

static int cmp_dec(const struct item &a, const struct item &b)
{
        return a.size > b.size;
}

static void sort_dec(vector<struct item> &lst_itms)
{
        sort(lst_itms.begin(), lst_itms.end(), cmp_dec);
}

static int gen_rand(int min, int max) 
{
        random_device rd;
        mt19937 gen(rd());
        uniform_int_distribution<> distr(min, max);

        return distr(gen);
}

static void assign_id(vector<struct item> &lst_itms, struct params &prm)
{
        for (int i = 0; i < prm.n; i++)
                lst_itms[i].id = i;
}

static void find_duplicate(int tmp[], int &lf, int &rf, 
                vector<struct item> &lst_itms, int cut_nbr, 
                int itm_idx, int cut_idx)
{
        for (int i = 0; i < cut_nbr; i++) {
                /* don't iterate the entire array */
                if (tmp[i] == 0) 
                        break;

                while (lf == tmp[i]) {
                        lf = 0;
                        rf = 0;
                        lf = gen_rand(1, lst_itms[itm_idx].size - 1);
                        rf = lst_itms[itm_idx].size - lf;
                }
                lst_itms[itm_idx].lst_cuts[cut_idx].c_pair.first = lf;
                lst_itms[itm_idx].lst_cuts[cut_idx].c_pair.second = rf;
        }
}

static void gen_frag_size(int size, int &lf_tmp, int &rf_tmp, struct params &prm)
{
        lf_tmp = gen_rand(1, size - 1); 
        rf_tmp = size - lf_tmp;

        while (lf_tmp > prm.phi || rf_tmp > prm.phi) {
                lf_tmp = gen_rand(1, size - 1);
                rf_tmp = size - lf_tmp;
        }
}

static void gen_itm_cut_nbr(struct item &itm, struct params &prm)
{
        switch (prm.cp) {
                case 0:
                        itm.nbr_cut = 0;
                        break;

                default:
                        //itm.nbr_cut = gen_rand(0, prm.cp);
                        itm.nbr_cut = gen_rand(1, prm.cp);
                        break;
        }
}

static void gen_itm_size(struct item &itm, struct params &prm)
{
        switch (prm.cp) {
                case 0:
                        itm.size = gen_rand(1, prm.phi);
                        break;

                default:
                        //if (itm.nbr_cut == 0)
                        //        itm.size = gen_rand(1, prm.phi);
                        //else
                        itm.size = gen_rand(itm.nbr_cut + 1, prm.s);
                        break;
        }
}

static void gen_cut_pairs(vector<struct item> &lst_itms, int &i, struct params &prm)
{
        int lf;
        int rf;
        int tmp[prm.cp];
        struct cut cut;

        memset(tmp, 0, sizeof(tmp));
        for (int j = 0; j < lst_itms[i].nbr_cut; j++) {
                cut.id = j;
                cut.c_pair.first = 0;
                cut.c_pair.second = 0;
                lf = 0;
                rf = 0;

                lst_itms[i].lst_cuts.push_back(cut);

                gen_frag_size(lst_itms[i].size, lf, rf, prm);

                lst_itms[i].lst_cuts[j].c_pair.first = lf;
                lst_itms[i].lst_cuts[j].c_pair.second = rf;

                find_duplicate(tmp, lf, rf, lst_itms, lst_itms[i].nbr_cut, i, j);
                tmp[j] = lf;
        }
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

void comp_min_bins(vector<struct item> &lst_itms, struct context &ctx)
{
        for (int i = 0; i < ctx.prm.n; i++) 
                ctx.itms_size += lst_itms[i].size;

        ctx.bins_min = abs(ctx.itms_size / ctx.prm.phi) + 1;

        printf("Number of Items: %d\n", ctx.itms_nbr);
        printf("Total Size of Items: %u\n", ctx.itms_size);
        printf("Minimum Number of Bins Required: %u\n", ctx.bins_min);
}

void gen_item_set(vector<struct item> &lst_itms, struct params &prm)
{
        printf("\n\n");
        printf("+=====================================+\n");
        printf("| INSTANCE GENERATION                 |\n");
        printf("+=====================================+\n");

        for (int i = 0; i < prm.n; i++) {
                struct item itm;
                itm.is_frag = NO;
                itm.is_allocated = NO;
                itm.is_fragmented = NO;

                gen_itm_cut_nbr(itm, prm);
                gen_itm_size(itm, prm);

                lst_itms.push_back(itm);

                if (lst_itms[i].nbr_cut > 0) 
                        gen_cut_pairs(lst_itms, i, prm);
        }
        sort_dec(lst_itms);
        assign_id(lst_itms, prm);
}

void gen_tc_set(vector<struct task_chain> &v_tc, struct params &prm)
{
        int task_nbr;
        int ncount = 0;
        int phicount = 0;
        int lf_size = 0;
        int rf_size = 0;

        /* generate task-chains set */
        while (ncount != prm.n) {
                struct task_chain tc;
                tc.u = 0;
                task_nbr = gen_rand(MINTASKNBR, MAXTASKNBR);

                for (int i = 0; i < task_nbr; i++) {
                        struct task tau;
                        tau.u = gen_rand(1, prm.s);
                        tau.c = gen_rand(MINWCET, MAXWCET);
                        tau.t = ceilf(((float)tau.c/(float)tau.u) * PERCENT);
                        tau.d = tau.t; /* implicit deadline */
                        tau.r = 0;
                        tau.p = i + 1; /* 1 is highest priority */
                        tau.id = i;

                        tc.v_tasks.push_back(tau);
                        tc.u += tau.u;
                }

                if (wcrt(tc.v_tasks) == SCHED_FAILED) {
                        continue;

                } else if (tc.u <= prm.c && phicount != prm.fr && tc.u > prm.phi) {
                        v_tc.push_back(tc);
                        ncount++;
                        phicount++;
                        continue;

                } else if (tc.u <= prm.c && phicount == prm.fr) {
                        v_tc.push_back(tc);
                        ncount++;
                        continue;
                }
        }

        printf("\n");

        /* generate cuts for each chain */
        for (unsigned int i = 0; i < v_tc.size(); i++) {
                /* iterate over tasks */
                for (unsigned int j = 0; j < v_tc[i].v_tasks.size() - 1; j++) {
                        struct cut c;
                        lf_size += v_tc[i].v_tasks[j].u;
                        rf_size = v_tc[i].u - lf_size;
                        c.id = j;
                        c.c_pair.first = lf_size;
                        c.c_pair.second = rf_size;

                        /* copy tasks to left fragment */
                        for (unsigned int k = j; k >= 0; k--) {
                                c.v_tasks_lf.push_back(v_tc[i].v_tasks[k]);
                                /* for the first task */
                                if (k == 0)
                                        break;
                        }

                        /* copy tasks to right fragment */
                        for (unsigned int k = j + 1; k <= v_tc[i].v_tasks.size() - 1; k++) {
                                c.v_tasks_rf.push_back(v_tc[i].v_tasks[k]);
                        }
                        v_tc[i].lst_cuts.push_back(c);
                }
                lf_size = 0;
                rf_size = 0;
        }
}
