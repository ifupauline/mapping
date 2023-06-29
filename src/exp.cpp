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
                case B_M: 
                        for (int i = 0; i < size; i++)
                                fprintf(filename, "%d %f\n", 
                                                data[i].sig, 
                                                data[i].mean_m);
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

void print_b_stats(vector<struct b_stats> &v_stts_algo, int iter)
{
        for (int i = 0; i < iter; i++) {
                printf("+===============+\n");
                printf("| SIG: %d       |\n", v_stts_algo[i].sig);
                printf("+===============+\n");
                printf("M: %f\n", v_stts_algo[i].mean_m);
                
                printf("\n");
        }
}
