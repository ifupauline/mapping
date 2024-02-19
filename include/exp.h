#ifndef EXP_H
#define EXP_H

#define B_M        0
#define B_ET       1
#define B_SR_ALLO  2
#define B_SR_DISP  3
#define B_SR_SWAP  4

#define BUFFSIZE  256

#include "mapping.h"

struct b_stats {
        float sig;
        float bfdu_m;
        float wfdu_m;
        float ffdu_m;
        float bfdu_et;
        float wfdu_et;
        float ffdu_et;
        float bfdu_sr_allo;
        float wfdu_sr_allo;
        float ffdu_sr_allo;
        float bfdu_sr_disp;
        float wfdu_sr_disp;
        float ffdu_sr_disp;
        float bfdu_sr_swap;
        float wfdu_sr_swap;
        float ffdu_sr_swap;
};

void concatenate(char p[], char q[]);

void write_to_file(FILE *filename, vector<float> &v_float);

void write_data_to_file(FILE *filename, vector<struct b_stats> &data,
                int type, int size);

void print_b_stats(vector<struct b_stats> &v_stts_algo, int iter);

#endif /* EXP.H */   
