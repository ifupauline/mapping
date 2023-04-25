#ifndef MAPPING_H 
#define MAPPING_H 

/* headers */
#include "time.h"
#include "stdio.h"
#include "string.h"

#include "model.h"

/* booleans */
#define NO  0
#define YES 1

/* algorithms */
#define BFDU_F 1
#define WFDU_F 2

#define C       1000
#define PERMILL 1000
#define PERCENT 100
#define MSEC    1000

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
        float allo_time;
        float wcrt_time;
        float reass_time;
        float disp_time;
        float swap_time;
        float sched_rate_allo;
        float sched_rate_prio;
        float sched_rate_disp;
        float sched_rate_swap;
        float sched_rate_opti;
        float reas_gain;
        float disp_gain;
        float swap_gain;
        int sched_imp_prio;
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

/* mapping */
void generation(vector<struct bin> &v_bins, struct context &ctx);

void allocation(vector<struct item> &v_itms, vector<struct bin> &v_bins, 
                struct context &ctx);

void schedulability_analysis(vector<struct bin> &v_bins, struct context &ctx);

void optimization(vector<struct bin> &v_bins, struct context &ctx);

/* algorithms */
void bfdu_f(vector<struct item> &v_itms, vector<struct bin> &v_bins, 
                struct context &ctx);

void wfdu_f(vector<struct item> &v_itms, vector<struct bin> &v_bins, 
                struct context &ctx);

/* getters */
vector<struct item> *get_frags_bfdu_f(void);

vector<struct item> *get_frags_wfdu_f(void);

#endif /* MAPPING.H */
