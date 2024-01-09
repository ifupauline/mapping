#ifndef PRINT_H
#define PRINT_H

#include "model.h"

float sched_rate(vector<struct bin> &v_bins, struct context &ctx);

void cmp_stats(vector<struct bin> &v_bins, vector<struct item> &v_itms, 
                struct context &ctx);

void print_task_chains(vector<struct item> &v_itms);

void print_core(struct bin &b);

void print_cores(vector<struct bin> &v_bins, struct context &ctx);

void print_stats(vector<struct item> &v_itms, vector<struct bin> &v_bins, 
                struct context &ctx);

void print_vectors(vector<struct bin> &v_bins, vector<struct item> &v_itms, 
                struct context &ctx);

#endif /* PRINT_H */
