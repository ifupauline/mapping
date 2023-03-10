#ifndef BENCH_H
#define BENCH_H

#define B_CR      0
#define B_CC      1
#define B_ET      2
#define B_CC_OPTI 3
#define B_CC_ALLO 4
#define B_ET_ALLO 5
#define B_ET_OPTI 6
#define B_SR_ALLO 7
#define B_SR_AUGM 8
#define B_SR_OPTI 9

#define BUFFSIZE  256

#include "mapping.h"

struct b_stats {
        int phi;
        float mean_cc;
        float mean_cr;
        float mean_et;
        float mean_cc_opti;
        float mean_cc_allo;
        float mean_sr_allo;
        float mean_sr_opti;
        float mean_sr_augm;
};

void concatenate(char p[], char q[]);

void write_data_to_file(FILE *filename, vector<struct b_stats> &data,
                int type, int size);

void plot_data(FILE *gnuplot_pipe, char const *commands_gnuplot[], int cmd_nbr);

void print_b_stats(vector<struct b_stats> &v_stts_bfdu_f, int iter);

#endif /* BENCH.H */
