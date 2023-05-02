#ifndef SCHED_ANALYSIS_H
#define SCHED_ANALYSIS_H

#include <random>

#include "model.h"

#define SCHED_OK     1
#define SCHED_FAILED 2

void base_assignment(struct bin &b);

void reassignment(struct bin &b);

int wcrt(vector<struct task> &v_tasks);

void wcrt_bin(struct bin &b, int bin_idx);

void sched_analysis(vector<struct bin> &v_bins, struct context &ctx);

#endif /* SCHED_ANALYSIS_H */
