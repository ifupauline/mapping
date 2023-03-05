#ifndef SCHED_ANALYSIS_H
#define SCHED_ANALYSIS_H

#include <random>

#include "mapping.h"

#define SCHED_OK     1
#define SCHED_FAILED 2

void assign_unique_priorities(struct bin &b);

void assign_new_priorities(struct bin &b, int p, int itm_idx);

int wcrt(vector<struct task> &v_tasks);

void wcrt_bin(struct bin &b, int bin_idx);

void wcrt_v_bins(vector<struct bin> &v_bins, struct context &ctx);

float sched_rate(vector<struct bin> &v_bins, struct context &ctx);

#endif /* SCHED_ANALYSIS_H */
