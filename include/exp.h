#ifndef EXP_H
#define EXP_H

#define B_M       0

#define BUFFSIZE  256

#include "mapping.h"

struct b_stats {
        int sig;
        float mean_m;
};

void concatenate(char p[], char q[]);

void write_data_to_file(FILE *filename, vector<struct b_stats> &data,
                int type, int size);

void plot_data(FILE *gnuplot_pipe, char const *commands_gnuplot[], int cmd_nbr);

void print_b_stats(vector<struct b_stats> &v_stts_algo, int iter);

#endif /* EXP.H */   
