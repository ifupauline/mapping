#include "exp.h"

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
                case B_CR: 
                        for (int i = 0; i < size; i++)
                                fprintf(filename, "%f %f\n", 
                                                (float)data[i].phi, 
                                                data[i].mean_cr);
                        break;

                case B_ET:
                        for (int i = 0; i < size; i++)
                                fprintf(filename, "%f %f\n", 
                                                (float)data[i].phi, 
                                                data[i].mean_et);
                        break;

                case B_SR:
                        for (int i = 0; i < size; i++)
                                fprintf(filename, "%f %f\n", 
                                                (float)data[i].phi, 
                                                data[i].mean_sr);
                        break;

                default:
                        printf("Error! Unknown type of data\n");
        }
}

void plot_data(FILE *gnuplot_pipe, char const *commands_gnuplot[], int cmd_nbr)
{
        for (int i = 0; i < cmd_nbr; i++)
                fprintf(gnuplot_pipe, "%s\n", commands_gnuplot[i]);
}

void print_b_stats(vector<struct b_stats> &v_stts_bfdu_f, int iter)
{
        for (int i = 0; i < iter; i++) {
                printf("+===============+\n");
                printf("| PHI: %d         |\n", v_stts_bfdu_f[i].phi);
                printf("+===============+\n");
                printf("BFDU_F.Cores Ratio                : %f\n", v_stts_bfdu_f[i].mean_cr);
                printf("BFDU_F.Execution Time             : %f\n", v_stts_bfdu_f[i].mean_et);
                printf("BFDU_F.Schedulability Rate (bef)  : %f\n", v_stts_bfdu_f[i].mean_sr);
                printf("\n");
        }
}
