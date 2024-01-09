#ifndef MAPPING_H 
#define MAPPING_H 

/* headers */
#include "time.h"
#include "stdio.h"
#include "string.h"

#include "model.h"

/* mapping */
void partitioning(vector<struct tc> &v_tcs, struct context &ctx);

void allocation(vector<struct tc> &v_tcs, vector<struct core> &v_cores, 
                struct context &ctx);

void schedulability_analysis(vector<struct core> &v_cores, struct context &ctx);

void placement(vector<struct core> &v_cores, struct context &ctx);

/* algorithms */
void bfdu_f(vector<struct tc> &v_tcs, vector<struct core> &v_cores, 
                struct context &ctx);

void wfdu_f(vector<struct tc> &v_tcs, vector<struct core> &v_cores, 
                struct context &ctx);

void ffdu_f(vector<struct tc> &v_tcs, vector<struct core> &v_cores, 
                struct context &ctx);

/* getters */
vector<struct tc> *get_frags_bfdu_f(void);

vector<struct tc> *get_frags_wfdu_f(void);

#endif /* MAPPING.H */
