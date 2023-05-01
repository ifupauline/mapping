#ifndef SCHED_ANALYSIS_H
#define SCHED_ANALYSIS_H

#include <random>

#include "model.h"

#define SCHED_OK     1
#define SCHED_FAILED 2

void base_assignment(struct bin &b);

void assign_new_priorities(struct bin &b, int p, int itm_idx);

int wcrt(vector<struct task> &v_tasks);

void wcrt_bin(struct bin &b, int bin_idx);

void sched_analysis(vector<struct bin> &v_bins, struct context &ctx);

void reassign(struct bin &b, int &p, int itm_idx);

void reassign_bin(struct bin &b);

void reassignment(vector<struct bin> &v_bins);

float sched_rate(vector<struct bin> &v_bins, struct context &ctx);

#endif /* SCHED_ANALYSIS_H */
