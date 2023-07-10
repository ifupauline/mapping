#ifndef EXP_H
#define EXP_H

#define B_M        0
#define B_SR_ALLO  1
#define B_SR_DISP  2
#define B_SR_SWAP  3

#define BUFFSIZE  256

#include "mapping.h"

struct b_stats {
        int sig;
        float mean_bfdu_m;
        float mean_wfdu_m;
        float mean_ffdu_m;
        float mean_bfdu_sr_allo;
        float mean_wfdu_sr_allo;
        float mean_ffdu_sr_allo;
        float mean_bfdu_sr_disp;
        float mean_wfdu_sr_disp;
        float mean_ffdu_sr_disp;
        float mean_bfdu_sr_swap;
        float mean_wfdu_sr_swap;
        float mean_ffdu_sr_swap;
};

void concatenate(char p[], char q[]);

void write_data_to_file(FILE *filename, vector<struct b_stats> &data,
                int type, int size);

void plot_data(FILE *gnuplot_pipe, char const *commands_gnuplot[], int cmd_nbr);

void print_b_stats(vector<struct b_stats> &v_stts_algo, int iter);

#endif /* EXP.H */   
