#ifndef SCHED_ANALYSIS_H
#define SCHED_ANALYSIS_H

#include "stdio.h"
#include "math.h"

#include <vector>
#include <algorithm>

#include "mapping.h"

#define SCHED_OK     1
#define SCHED_FAILED 2

using namespace std;

int wcrt(vector<struct task> &v_tasks);

int sched_analysis(vector<struct bin> &v_bins, struct context &ctx);

void priority_assignment(vector<struct bin> &v_bins);

void priority_reassignment(struct bin &b, unsigned int &j, int &p);

#endif /* SCHED_ANALYSIS_H */
