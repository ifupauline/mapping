#ifndef MODEL_H
#define MODEL_H

#include <vector>
#include <algorithm>
#include <bits/stdc++.h>

/* algorithms */
#define BFDU_F 1
#define WFDU_F 2
#define FFDU_F 3

/* booleans */
#define NO  1
#define YES 2

#define LEFT  1
#define RIGHT 2

/* core filling capacity */
#define C       1000
#define PHI     800

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
#define WHITE  6

using namespace std;

struct t_pos {
        int core_idx;
        int tc_idx;
        int task_idx;
};

struct task {
        int c;
        int t;
        int p;
        int r;
        int id;
        int uniq_id;
        int tc_id;
        int is_let;
        float u;
        struct t_pos idx;
};

struct colorsize {
        int red;
        int blue;
        int yellow;
        int green;
        int cyan;
        int purple;
        int white;
        int red_cores_min;
        int blue_cores_min;
        int yellow_cores_min;
        int green_cores_min;
        int cyan_cores_min;
        int purple_cores_min;
        int white_cores_min;
};

struct tc {
        int id;
        int tc_idx;
        int size;
        int gcd;
        int memcost;
        int disp_count;
        int swap_count;
        int color;
        int is_let;
        int is_frag;
        int is_allocated;
        vector<struct task> v_tasks;
};

struct core {
        int id;
        int phi;
        int flag;
        int load;
        int load_rem;
        int color;
        int memcost;
        vector<struct tc> v_tcs;
        vector<struct task> v_tasks;
};

struct params {
        int a;
        int n;
        int s;
};

struct perf {
        float m;
        float ar;
        float et;
        float letu;
        float appu;
        float unuu;
        float maxu;
        float part_time;
        float allo_time;
        float schd_time;
        float disp_time;
        float swap_time;
        float plac_time;
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
        int tcs_nbr;
        int tcs_size;
        int cores_min;
        int cores_count;
        int tcs_count;
        int alloc_count;
        int tasks_count;
        int frags_count;
        int sched_ok_count;
        int sched_failed_count;
        struct perf p;
        struct params prm;
        struct colorsize cs;
};

/* OPERATIONS ON DATA STRUCTURES */
void sort_inc_task_priority(vector<struct task> &v_tasks);

void sort_inc_task_id(vector<struct task> &v_tasks);

void sort_inc_core_load_rem(vector<struct core> &v_cores);

void sort_inc_core_color(vector<struct core> &v_cores);

void sort_dec_int(vector<int> &v_int);

void sort_dec_tc_size(vector<struct tc> &v_tcs);

void sort_inc_tc_id(vector<struct tc> &v_tcs);

void sort_inc_tc_tc_idx(vector<struct tc> &v_tcs);

void sort_inc_tc_color(vector<struct tc> &v_tcs);

void copy_back_prio_to_tc(struct core &b);

void copy_back_resp_to_tc(struct core &b);

void copy_v_tc_to_v_tasks_with_pos(vector<struct core> &v_cores);

void copy_tc_to_v_tasks_with_pos(struct core &b, int core_idx, int tc_idx);

void cmp_core_load(struct core &b, int &load);

void cmp_tc_load(struct tc &tc);

void cmp_core_load_rem(struct core &b);

void cmp_core_memcost(struct core &b);

int cmp_gcd(vector<struct task> &v_tasks);

void rplc_core_by_id(vector<struct core> &v_cores, struct core &b);

struct tc get_tc_by_id(vector<struct core> &v_cores, int tc_id);

struct core get_core_by_id(vector<struct core> &v_cores, int core_id);

int get_wcrt(struct core &b, int tc_id, int tc_idx);

int get_color_by_id(vector<struct core> &v_cores, int core_id);

int get_core_idx_by_id(vector<struct core> &v_cores, int core_id);

void del_tc_by_id(struct core &b, int tc_id, int tc_idx);

void add_tasks_to_v_tasks(vector<struct task> &dst_v_tasks, 
                vector<struct task> &src_v_tasks);

void add_core_color(vector<struct core> &v_cores, int color, struct context &ctx);

void add_tc_to_core(struct core &b, struct tc &tc, int load, int gcd);

void add_tc_to_v_cores(vector<struct core> &v_cores, struct tc &tc, int core_id, 
                struct context &ctx, int load, int gcd);
#endif /* MODEL_H */
