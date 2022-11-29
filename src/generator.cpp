#include "../include/mapping.h"

static void helper(void)
{
        printf("[N]         --> is number of items of the instance\n");
        printf("[S]         --> is the maximum size of an item\n");
        printf("[C]         --> is the bin capacity\n");
        printf("[k]         --> is a constraint on C, means that a bin cannot "
                        "be filled more than K\n");
        exit(0);
}

static void check_params(struct params &prm)
{
        if (prm.n <= 0) {
                printf("Wrong Value for N, cannot be <= %d\n", 0);
                helper();
        }

        if (prm.s <= 0 || prm.s > prm.c ||
                        (prm.s <= prm.cp)) {
                printf("Wrong Value for S, cannot be <= %d or > C "
                                "or <= cp\n", 0);
                helper();
        }

        if (prm.c < MAXC) {
                printf("Wrong Value for C, cannot be < %d\n", MAXC);
                helper();
        }

        if (prm.k > prm.c || prm.k < MINK) {
                printf("Wrong Value for k, cannot be >= C or < %d\n", MINK);
                helper();
        }

        if (prm.cp < 0 || prm.cp > MAXCP) {
                printf("Wrong Value for cp, %d < cp <= %d\n", 0, MAXCP);
                helper();
        }
}

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

        while (lf_tmp > prm.k || rf_tmp > prm.k) {
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
                        itm.size = gen_rand(1, prm.k);
                        break;

                default:
                        //if (itm.nbr_cut == 0)
                        //        itm.size = gen_rand(1, prm.k);
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
        ctx.frag_rate = 0.0;
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

        ctx.bins_min = abs(ctx.itms_size / ctx.prm.k) + 1;

        printf("Number of Items: %d\n", ctx.itms_nbr);
        printf("Total Size of Items: %u\n", ctx.itms_size);
        printf("Minimum Number of Bins Required: %u\n", ctx.bins_min);
}

void gen_data_set(vector<struct item> &lst_itms, struct params &prm)
{
        printf("\n\n");
        printf("+=====================================+\n");
        printf("| INSTANCE GENERATION                 |\n");
        printf("+=====================================+\n");

        check_params(prm);
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
