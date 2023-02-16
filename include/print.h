#ifndef PRINT_H
#define PRINT_H

#include "mapping.h"
#include "sched_analysis.h"

#define MSEC 1000

void print_not_allocated(vector<struct item> &lst_itms);

void print_not_fragmented(vector<struct item> &lst_itms);

void print_lst_itms(vector<struct item> &lst_itms, struct context &ctx);

void print_lst_bins(vector<struct bin> &lst_bins, struct context &ctx);

void print_stats(vector<struct item> &lst_itms, vector<struct bin> &lst_bins, 
                struct context &ctx);

void print_vectors(vector<struct item> &lst_itms, struct context &ctx);

void print_task_chains(vector<struct task_chain> &v_tc);

#endif /* PRINT_H */
