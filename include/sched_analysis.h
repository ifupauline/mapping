#ifndef SCHED_ANALYSIS_H
#define SCHED_ANALYSIS_H

#include <random>

#include "model.h"

#define SCHED_OK     1
#define SCHED_FAILED 2

#define BFCT 3   /* blocking factor - max nbr of interfering cores */
#define WCBT 250 /* worst-case blocking time from one core -  microsecs */

void priority_assignment(struct core &b);

int wcrt(vector<struct task> &v_tasks);

int wcrt_part(vector<struct task> &v_tasks);

void sched_analysis(vector<struct core> &v_cores, struct context &ctx);

#endif /* SCHED_ANALYSIS_H */
