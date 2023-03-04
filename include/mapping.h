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

#define C       100
#define PERCENT 100

struct params {
        int n;
        int phi;
        int a;
        int h;
};

struct perf {
        float redu_time;
        float alloc_time;
        float e_time;
        float wca_time;
        float reass_time;
        float disp_time;
        float opti_bins;
        float sched_rate_bef;
        float sched_rate_prio;
        float sched_rate_disp;
        int sched_imp_prio;
        int sched_imp_disp;
        int sched_imp_tc_prio;
        int sched_imp_tc_disp;
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
        int sched_tcok_count;
        int sched_tcfailed_count;
        struct perf p;
        struct params prm;
};

/* mapping */
void generation(vector<struct bin> &v_bins, struct context &ctx);

void reduction(vector<struct item> &v_itms, vector<struct bin> &v_bins, 
                struct context &ctx);

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
