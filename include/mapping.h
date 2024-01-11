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
void bfdu(vector<struct tc> &v_tcs, vector<struct core> &v_cores, 
                struct context &ctx);

void wfdu(vector<struct tc> &v_tcs, vector<struct core> &v_cores, 
                struct context &ctx);

void ffdu(vector<struct tc> &v_tcs, vector<struct core> &v_cores, 
                struct context &ctx);

#endif /* MAPPING.H */
