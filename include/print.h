#ifndef PRINT_H
#define PRINT_H

#include "mapping.h"
#include "sched_analysis.h"

#define MSEC 1000

void print_not_allocated(vector<struct item> &v_itms);

void print_not_fragmented(vector<struct item> &v_itms);

void print_v_itms(vector<struct item> &v_itms, struct context &ctx);

void print_v_bins(vector<struct bin> &v_bins, struct context &ctx);

void print_stats(vector<struct item> &v_itms, vector<struct bin> &v_bins, 
                struct context &ctx);

void print_vectors(vector<struct item> &v_itms, struct context &ctx);

void print_task_chains(vector<struct item> &v_itms);

#endif /* PRINT_H */
