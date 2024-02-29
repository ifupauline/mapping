#ifndef STATS_H
#define STATS_H

#include "model.h"

float sched_rate(vector<struct core> &v_cores, struct context &ctx);

void stats(vector<struct core> &v_cores, vector<struct tc> &v_tcs, 
                struct context &ctx);

#endif /* STATS_H */
