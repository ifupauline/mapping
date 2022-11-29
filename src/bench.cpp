#include "../include/bench.h"

void concatenate(char p[], char q[]) {
        int c;
        int d;
        c = 0;

        while (p[c] != '\0') {
                c++;
        }
        d = 0;

        while (q[d] != '\0') {
                p[c] = q[d];
                d++;
                c++;
        }
        p[c] = '\0';
}

void write_data_to_file(FILE *filename, vector<struct b_stats> &data,
                int type, int size)
{

        switch (type) {
                case B_AR: 
                        for (int i = 0; i < size; i++)
                                fprintf(filename, "%f %f \n", 
                                                (float)data[i].k, data[i].mean_ar);
                        break;

                case B_ET:
                        for (int i = 0; i < size; i++)
                                fprintf(filename, "%f %f \n", 
                                                (float)data[i].k, data[i].mean_et);
                        break;

                case B_LD:
                        for (int i = 0; i < size; i++)
                                fprintf(filename, "%f %f \n", 
                                                (float)data[i].k, data[i].mean_ld);
                        break;

                case B_FR:
                        for (int i = 0; i < size; i++)
                                fprintf(filename, "%f %f \n", 
                                                (float)data[i].k, data[i].mean_fr);
                        break;

                default:
                        printf("Error! Unknown type of data\n");
        }
}

void plot_data(FILE *gnuplot_pipe, char const *commands_gnuplot[], int cmd_nbr)
{
        for (int i = 0; i < cmd_nbr; i++)
                fprintf(gnuplot_pipe, "%s \n", commands_gnuplot[i]);
}

void print_b_stats(vector<struct b_stats> &v_stts_bfdu_f, 
                vector<struct b_stats> &v_stts_wfdu_f, int iter)
{
        for (int i = 0; i < iter; i++) {
                printf("+===============+\n");
                printf("| K: %d         |\n", v_stts_bfdu_f[i].k);
                printf("+===============+\n");
                printf("BFDU_F.Approximation Ratio (Bins) : %f\n", v_stts_bfdu_f[i].mean_ar);
                printf("WFDU_F.Approximation Ratio (Bins) : %f\n\n", v_stts_wfdu_f[i].mean_ar);
                printf("BFDU_F.Execution Time (ms)        : %f\n", v_stts_bfdu_f[i].mean_et);
                printf("WFDU_F.Execution Time (ms)        : %f\n\n", v_stts_wfdu_f[i].mean_et);
                printf("BFDU_F.Load Distribution (std_dev): %f\n", v_stts_bfdu_f[i].mean_ld);
                printf("WFDU_F.Load Distribution (std_dev): %f\n\n", v_stts_wfdu_f[i].mean_ld);
                printf("BFDU_F.Fragmentation Rate (per)   : %f\n", v_stts_bfdu_f[i].mean_fr);
                printf("WFDU_F.Fragmentation Rate (per)   : %f\n", v_stts_wfdu_f[i].mean_fr);
                printf("\n");
        }
}
