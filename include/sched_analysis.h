#ifndef SCHED_ANALYSIS_H
#define SCHED_ANALYSIS_H

#include "stdio.h"
#include "math.h"

#include <vector>
#include <algorithm>

#define SCHED_OK     1
#define SCHED_FAILED 2

using namespace std;

struct task {
        int c;
        int t;
        int d;
        int u;
        int p;
        int r;
        int id;
};

struct task_chain {
        int u;
        vector<struct task> v_tasks;
};

int wcrt(vector<struct task> &v_tasks);

#endif /* SCHED_ANALYSIS_H */
