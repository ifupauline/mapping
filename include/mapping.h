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
#define BFDU_F 1
#define WFDU_F 2

using namespace std;

struct params {
        int n;
        int c;
        int max_tu;
        int phi;
        int fr;
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
        int sched_ok_count;
        int sched_failed_count;
        float redu_time;
        float alloc_time;
        float e_time;
        float sched_time;
        float standard_dev;
        float opti_bins;
        struct params prm;
};

struct t_idx {
        int bin_idx;
        int itm_idx;
        int task_idx;
};

struct task {
        int c;
        int t;
        int d;
        int u;
        int p;
        int r;
        int id;
        struct t_idx idx;
};

struct cut {
        int id;
        pair<int, int> c_pair;
        vector<struct task> v_tasks_lf;
        vector<struct task> v_tasks_rf;
};

struct task_chain {
        int u;
        vector<struct task> v_tasks;
        vector<struct cut> v_cuts;
};

struct item {
        int id;
        int size;
        int nbr_cut;
        int is_frag;
        int is_allocated;
        int is_fragmented;
        struct task_chain tc;
};

struct bin {
        int id;
        int flag;
        int cap_rem;
        vector<struct item> v_itms;
        vector<struct task> v_tasks;
};

/* mapping */
void generation(vector<struct bin> &v_bins, struct context &ctx);

void reduction(vector<struct item> &v_itms, vector<struct bin> &v_bins, 
                struct context &ctx);

void allocation(vector<struct item> &v_itms, vector<struct bin> &v_bins, 
                struct context &ctx);

void worst_case_analysis(vector<struct bin> &v_bins, struct context &ctx);

/* operations */
void add_bin(vector<struct bin> &v_bins, struct context &ctx);

void add_itm_to_bin(vector<struct bin> &v_bins, struct item &itm, int bin_id, 
                struct context &ctx);

/* algorithms */
void bfdu_f(vector<struct item> &v_itms, vector<struct bin> &v_bins, 
                struct context &ctx);

void wfdu_f(vector<struct item> &v_itms, vector<struct bin> &v_bins, 
                struct context &ctx);

/* getters */
vector<struct item> *get_frags_bfdu_f(void);

vector<struct item> *get_frags_wfdu_f(void);

#endif /* MAPPING.H */
