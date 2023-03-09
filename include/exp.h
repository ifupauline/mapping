#ifndef BENCH_H
#define BENCH_H

#define B_CR      0
#define B_ET      1
#define B_ET_ALLO 2
#define B_ET_OPTI 3
#define B_SR_ALLO 4
#define B_SR_AUGM 5
#define B_SR_OPTI 6

#define BUFFSIZE  256

#include "mapping.h"

struct b_stats {
        int phi;
        float mean_cr;
        float mean_et;
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
