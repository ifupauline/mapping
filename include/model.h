#ifndef MODEL_H
#define MODEL_H

#include <vector>
#include <algorithm>
#include<bits/stdc++.h>

/* algorithms */
#define BFDU_F 1
#define WFDU_F 2

/* booleans */
#define NO  0
#define YES 1

#define LEFT  0
#define RIGHT 1

#define C       1000
#define MINPHI  C/2
#define MAXPHI  C

/* units */
#define PERMILL 1000
#define PERCENT 100
#define MSEC    1000

/* mem cost of tc */
#define MINMEMCOST 0 /* no shared memory used */
#define MAXMEMCOST 3

using namespace std;

/* CONTEXT */
struct params {
        int a;
        int h;
        int n;
        int phi;
};

struct perf {
        float cr;
        float et;
        float fr;
        float let;
        float sys;
        float unu;
        float maxu;
        float allo_time;
        float schd_time;
        float disp_time;
        float swap_time;
        float sched_rate_base;
        float sched_rate_reas;
        float sched_rate_disp;
        float sched_rate_swap;
        float sched_rate_opti;
        float reas_gain;
        float disp_gain;
        float swap_gain;
        int sched_imp_reas;
        int sched_imp_disp;
        int sched_imp_swap;
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
        int tasks_count;
        int sched_ok_count;
        int sched_failed_count;
        struct perf p;
        struct params prm;
};

/* TASK MODEL */
struct t_pos {
        int bin_idx;
        int itm_idx;
        int task_idx;
};

struct task {
        int c;
        int t;
        int d;
        int p;
        int r;
        int id;
        int is_let;
        float u;
        struct t_pos idx;
};

struct cut {
        int id;
        pair<int, int> c_pair;
        vector<struct task> v_tasks_lf;
        vector<struct task> v_tasks_rf;
};

/* BIN-PACKING MODEL */
struct item {
        int id;
        int size;
        int gcd;
        int nbr_cut;
        int frag_id;
        int memcost;
        int disp_count;
        int swap_count;
        int is_let;
        int is_frag;
        int is_allocated;
        int is_fragmented;
        vector<struct task> v_tasks;
        vector<struct cut> v_cuts;
};

struct bin {
        int id;
        int phi;
        int flag;
        int load;
        int load_rem;
        int memcost;
        vector<struct item> v_itms;
        vector<struct task> v_tasks;
};

/* OPERATIONS ON DATA STRUCTURES */

void sort_inc_task_priority(vector<struct task> &v_tasks);

void sort_inc_task_id(vector<struct task> &v_tasks);

void sort_dec_itm_size(vector<struct item> &v_itms);

void sort_inc_bin_load_rem(vector<struct bin> &v_bins);

void copy_back_prio_to_tc(struct bin &b);

void copy_back_resp_to_tc(struct bin &b);

void copy_tc_to_v_tasks_with_pos(struct bin &b, int bin_idx, int itm_idx);

void compute_bin_load(struct bin &b, int &load);

void compute_itm_load(struct item &itm);

void compute_bin_load_rem(struct bin &b);

void compute_bin_memcost(struct bin &b);

int compute_gcd(vector<struct task> &v_tasks);

void add_bin(vector<struct bin> &v_bins, struct context &ctx);

void add_itm_to_v_bins(vector<struct bin> &v_bins, struct item &itm, int bin_id, 
                struct context &ctx, int load, int gcd);

void replace_bin_by_id(vector<struct bin> &v_bins, struct bin &b);

struct item retrieve_tc_by_id(vector<struct bin> &v_bins, int tc_id);

struct bin retrieve_core_by_id(vector<struct bin> &v_bins, int bin_id);

int get_bin_idx_by_id(vector<struct bin> &v_bins, int bin_id);

void delete_itm_by_id(struct bin &b, int itm_id);

void insert_itm_to_core(struct bin &b, struct item &itm);

void add_tasks_to_v_tasks(vector<struct task> &dst_v_tasks, 
                vector<struct task> &src_v_tasks);

#endif /* MODEL_H */
