#ifndef MODEL_H
#define MODEL_H

#include <vector>
#include <algorithm>
#include <bits/stdc++.h>

/* algorithms */
#define BFDU_F 1
#define WFDU_F 2
#define FRST_F 3

/* booleans */
#define NO  1
#define YES 2

#define LEFT  1
#define RIGHT 2

#define C       1000
#define PHI     800
#define EPSILON 500

/* units */
#define PERCENT 100
#define PERMILL 1000
#define MSEC    1000

/* mem cost of tc */
#define MINMEMCOST 1
#define MAXMEMCOST 3

/* colors */
#define RED    0
#define BLUE   1
#define YELLOW 2
#define GREEN  3
#define CYAN   4
#define PURPLE 5

using namespace std;

extern int bfdu_wcrt_count;
extern int bfdu_syst_state;
extern float bfdu_sched_time;

extern int wfdu_wcrt_count;
extern int wfdu_syst_state;
extern float wfdu_sched_time;

extern int frst_wcrt_count;
extern int fsrt_syst_state;
extern float frst_sched_time;

/* CONTEXT */
struct params {
        int a;
        int n;
        int e;
};

struct perf {
        float cr;
        float et;
        float fr;
        float let;
        float sys;
        float unu;
        float maxu;
        float frag_time;
        float allo_time;
        float schd_time;
        float disp_time;
        float swap_time;
        float comm_time;
        float sched_rate_tc;
        float sched_rate_allo;
        float sched_rate_disp;
        float sched_rate_swap;
        float sched_rate_opti;
        float disp_gain;
        float swap_gain;
        float opti_gain;
        int sched_imp_allo;
        int sched_imp_disp;
        int sched_imp_swap;
};

struct context {
        int itms_nbr;
        int itms_size;
        int bins_min;
        int bins_count;
        int cycl_count;
        int itms_count;
        int alloc_count;
        int cuts_count;
        int tasks_count;
        int frags_count;
        int comm_count;
        int e2e_ok_count;
        int e2e_failed_count;
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
        int uniq_id;
        int tc_id;
        int is_let;
        float u;
        struct t_pos idx;
};

/* BIN-PACKING MODEL */
struct item {
        int id;
        int tc_idx;
        int size;
        int gcd;
        int memcost;
        int disp_count;
        int swap_count;
        int e2ed;
        int color;
        int is_let;
        int is_frag;
        int is_allocated;
        vector<struct task> v_tasks;
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

void sort_inc_bin_load_rem(vector<struct bin> &v_bins);

void sort_dec_int(vector<int> &v_int);

void sort_dec_itm_size(vector<struct item> &v_itms);

void sort_inc_itm_id(vector<struct item> &v_itms);

void sort_inc_itm_tc_idx(vector<struct item> &v_itms);

void copy_back_prio_to_tc(struct bin &b);

void copy_back_resp_to_tc(struct bin &b);

void copy_v_tc_to_v_tasks_with_pos(vector<struct bin> &v_bins);

void copy_tc_to_v_tasks_with_pos(struct bin &b, int bin_idx, int itm_idx);

void compute_bin_load(struct bin &b, int &load);

void compute_itm_load(struct item &itm);

void compute_bin_load_rem(struct bin &b);

void compute_bin_memcost(struct bin &b);

int compute_gcd(vector<struct task> &v_tasks);

void add_bin(vector<struct bin> &v_bins, struct context &ctx);

void add_itm_to_bin(struct bin &b, struct item &itm, int load, int gcd);

void add_itm_to_v_bins(vector<struct bin> &v_bins, struct item &itm, int bin_id, 
                struct context &ctx, int load, int gcd);

void replace_bin_by_id(vector<struct bin> &v_bins, struct bin &b);

struct item retrieve_tc_by_id(vector<struct bin> &v_bins, int tc_id);

struct bin retrieve_core_by_id(vector<struct bin> &v_bins, int bin_id);

int retrieve_wcrt(struct bin &b, int itm_id, int tc_idx);

int get_bin_idx_by_id(vector<struct bin> &v_bins, int bin_id);

void delete_itm_by_id(struct bin &b, int itm_id, int itm_idx);

void add_tasks_to_v_tasks(vector<struct task> &dst_v_tasks, 
                vector<struct task> &src_v_tasks);

void check_duplicata(vector<struct bin> &v_bins);

int is_frag_same_tc(struct bin &b);

int is_task_same_v_tasks(struct bin &b);

#endif /* MODEL_H */
