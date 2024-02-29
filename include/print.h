#ifndef PRINT_H
#define PRINT_H

#include "model.h"

float sched_rate(vector<struct core> &v_cores, struct context &ctx);

void stats(vector<struct core> &v_cores, vector<struct tc> &v_tcs, 
                struct context &ctx);

void print_task_chains(vector<struct tc> &v_tcs);

void print_core(struct core &b);

void print_cores(vector<struct core> &v_cores, struct context &ctx);

void print_stats(vector<struct tc> &v_tcs, vector<struct core> &v_cores, 
                struct context &ctx);

void print_vectors(vector<struct core> &v_cores, vector<struct tc> &v_tcs, 
                struct context &ctx);

#endif /* PRINT_H */
