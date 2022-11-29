#ifndef MAPPING_H 
#define MAPPING_H 

/* headers */
#include "time.h"
#include "stdio.h"
#include "string.h"

#include <random>
#include <vector>
#include <algorithm>

/* booleans */
#define NO  1
#define YES 2

/* algorithms */
#define BFDU_F 0
#define WFDU_F 1

/* units */
#define MSEC 1000

/* params */
#define MINK  50
#define MAXCP 10
#define MAXC  100

using namespace std;

struct params {
        int n;
        int s;
        int c;
        int k;
        int cp;
        int a;
};

struct context {
        int bins_min;
        int bins_count;
        int cycl_count;
        int itms_count;
        int itms_nbr;
        int itms_size;
        int alloc_count;
        int frags_count;
        int cuts_count;
        float redu_time;
        float alloc_time;
        float frag_time;
        float e_time;
        float standard_dev;
        float opti_bins;
        float frag_rate;
        struct params prm;
};

struct cut {
        int id;
        pair<int, int> c_pair;
};

struct item {
        int id;
        int size;
        int nbr_cut;
        int is_frag;
        int is_allocated;
        int is_fragmented;
        vector<struct cut> lst_cuts;
};

struct bin {
        int id;
        int cap_rem;
        vector<struct item> vc_itms;
};

/* instance */
void gen_data_set(vector<struct item> &lst_itms, struct params &prm);

void comp_min_bins(vector<struct item> &lst_itms, struct context &ctx);

void comp_stats(vector<struct bin> &lst_bins, vector<struct item> &lst_itms, 
                struct context &ctx);

void init_ctx(struct params &prm, struct context &ctx);

/* mapping */
void generation(vector<struct bin> &lst_bins, struct context &ctx);

void reduction(vector<struct item> &lst_itms, vector<struct bin> &lst_bins, 
                struct context &ctx);

void allocation(vector<struct item> &lst_itms, vector<struct bin> &lst_bins, 
                struct context &ctx);

/* operations */
void add_bin(vector<struct bin> &lst_bins, struct context &ctx);

void add_itm_to_bin(vector<struct bin> &lst_bins, struct item &itm, int bin_id, 
                struct context &ctx);

/* algorithms */
void bfdu_f(vector<struct item> &lst_itms, vector<struct bin> &lst_bins, 
                struct context &ctx);

void wfdu_f(vector<struct item> &lst_itms, vector<struct bin> &lst_bins, 
                struct context &ctx);

/* getters */
vector<struct item> *get_frags_bfdu_f(void);

vector<struct item> *get_frags_wfdu_f(void);

#endif /* MAPPING.H */
