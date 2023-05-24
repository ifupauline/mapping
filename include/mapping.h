#ifndef MAPPING_H 
#define MAPPING_H 

/* headers */
#include "time.h"
#include "stdio.h"
#include "string.h"

#include "model.h"

/* mapping */
void fragmentation(vector<struct item> &v_itms, struct context &ctx);

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

void frst_f(vector<struct item> &v_itms, vector<struct bin> &v_bins, 
                struct context &ctx);

/* getters */
vector<struct item> *get_frags_bfdu_f(void);

vector<struct item> *get_frags_wfdu_f(void);

#endif /* MAPPING.H */
