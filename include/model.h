#ifndef MODEL_H
#define MODEL_H

#include <vector>

using namespace std;

/* TASK MODEL */
struct t_idx {
        int bin_idx;
        int itm_idx;
        int task_idx;
};

struct task {
        int c;
        int t;
        int d;
        int p;
        int r;
        int id;
        int u;
        struct t_idx idx;
};

struct cut {
        int id;
        pair<int, int> c_pair;
        vector<struct task> v_tasks_lf;
        vector<struct task> v_tasks_rf;
};

struct task_chain {
        int u;
        vector<struct task> v_tasks;
        vector<struct cut> v_cuts;
};

/* BIN-PACKING MODEL */
struct item {
        int id;
        int size;
        int nbr_cut;
        int is_frag;
        int is_allocated;
        int is_fragmented;
        struct task_chain tc;
};

struct bin {
        int id;
        int flag;
        int cap_rem;
        vector<struct item> v_itms;
        vector<struct task> v_tasks;
};

#endif /* MODEL_H */
