#ifndef MODEL_H
#define MODEL_H

#include <vector>
#include <algorithm>
#include <bits/stdc++.h>

/* algorithms */
#define BFDU 1
#define WFDU 2
#define FFDU 3

/* booleans */
#define NO  1
#define YES 2

#define OK     1
#define FAILED 2

#define LEFT  1
#define RIGHT 2

/* core filling capacity */
#define PHI 800

/* units */
#define PERCENT 100
#define PERMILL 1000
#define MSEC    1000

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
        int u;
        int task_id;
        int uniq_id;
        int tc_id;
        int is_let;
        struct t_pos idx;
};

struct tc {
        int id;
        int tc_idx;
        int u;
        int gcd;
        int color;
        int is_let;
        int is_frag;
        int is_assign;
        vector<struct task> v_tasks;
};

struct core {
        int id;
        int phi;
        int flag;
        int load;
        int load_rem;
        int color;
        int is_empty;
        vector<struct tc> v_tcs;
        vector<struct task> v_tasks;
};

struct params {
        int a;
        int m;
        int s;
};

struct perf {
        float m;
        float fr;
        float letu;
        float appu;
        float unuu;
        float sysu;
        float part_time;
        float assi_time;
        float schd_time;
        float disp_time;
        float swap_time;
        float plac_time;
        float exec_time;
        float sched_rate_tc;
        float sched_rate_allo;
        float sched_rate_swap;
        float sched_rate_disp;
        float sched_rate_opti;
        float swap_gain;
        float disp_gain;
        float plac_gain;
        int sched_imp_allo;
        int sched_imp_swap;
        int sched_imp_disp;
};

struct context {
        int k;
        int k_max;
        int p_arch;
        int tcs_count;
        int tasks_count;
        int cores_count;
        int pcu_cores_count;
        int zcu_cores_count;
        int sched_ok_count;
        int sched_failed_count;
        struct perf p;
        struct params prm;
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

int core_load(struct core &core);

void tc_load(struct tc &tc);

struct core get_core_by_id(vector<struct core> &v_cores, int core_id);

struct tc get_tc_by_id(vector<struct core> &v_cores, int tc_id);

int get_color_by_id(vector<struct core> &v_cores, int core_id);

int get_core_idx_by_id(vector<struct core> &v_cores, int core_id);

int get_duplicata(vector<int> &v_int);

void add_tasks_to_v_tasks(vector<struct task> &dst_v_tasks, 
                vector<struct task> &src_v_tasks);

void add_core(vector<struct core> &v_cores, int color, int speed_factor, 
                struct context &ctx);

void add_tc_to_core(struct core &b, struct tc &tc, int load, int gcd);

void add_tc_to_v_cores(vector<struct core> &v_cores, struct tc &tc, int core_id, 
                int load, int gcd);

void verify_prm(struct params &prm);

void verify_cores_load(vector<struct core> &v_cores);

void verify_pa(vector<struct core> &v_cores);

void reset_empty_cores(vector<struct core> &v_cores);

void reset_let_task(vector<struct core> &v_cores);

void ovrw_core_by_id(vector<struct core> &v_cores, struct core &b);

void rmv_tc_by_id(struct core &b, int tc_id, int tc_idx);

void assign_ids(vector<struct tc> &v_tcs);

int gcd(vector<struct task> &v_tasks);

#endif /* MODEL_H */
