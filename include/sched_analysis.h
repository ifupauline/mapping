#ifndef SCHED_ANALYSIS_H
#define SCHED_ANALYSIS_H

#include <random>

#include "mapping.h"

#define SCHED_OK     1
#define SCHED_FAILED 2

int wcrt(vector<struct task> &v_tasks);

void sched_analysis(vector<struct bin> &v_bins, struct context &ctx);

#endif /* SCHED_ANALYSIS_H */
