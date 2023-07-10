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
                                fprintf(filename, "%-3.2f %-3.2f %-3.2f %-3.2f\n", 
                                                data[i].sig, 
                                                data[i].mean_bfdu_m,
                                                data[i].mean_wfdu_m,
                                                data[i].mean_ffdu_m);
                        break;

                case B_SR_ALLO: 
                        for (int i = 0; i < size; i++)
                                fprintf(filename, "%f %f %f %f\n", 
                                                data[i].sig, 
                                                data[i].mean_bfdu_sr_allo,
                                                data[i].mean_wfdu_sr_allo,
                                                data[i].mean_ffdu_sr_allo);
                        break;

                case B_SR_DISP: 
                        for (int i = 0; i < size; i++)
                                fprintf(filename, "%f %f %f %f\n", 
                                                data[i].sig, 
                                                data[i].mean_bfdu_sr_disp,
                                                data[i].mean_wfdu_sr_disp,
                                                data[i].mean_ffdu_sr_disp);
                        break;

                case B_SR_SWAP: 
                        for (int i = 0; i < size; i++)
                                fprintf(filename, "%f %f %f %f\n", 
                                                data[i].sig, 
                                                data[i].mean_bfdu_sr_swap,
                                                data[i].mean_wfdu_sr_swap,
                                                data[i].mean_ffdu_sr_swap);
                        break;

                case B_ET: 
                        for (int i = 0; i < size; i++)
                                fprintf(filename, "%-3.2f %-3.2f\n", 
                                                data[i].sig, 
                                                data[i].mean_et);
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
                printf("| SIG: %d       |\n", (int)(v_stts_algo[i].sig * PERCENT));
                printf("+===============+\n");
                printf("BFDU_M:       %f\n", v_stts_algo[i].mean_bfdu_m);
                printf("WFDU_M:       %f\n", v_stts_algo[i].mean_wfdu_m);
                printf("FFDU_M:       %f\n", v_stts_algo[i].mean_ffdu_m);
                printf("BFDU_SR_ALLO: %f\n", v_stts_algo[i].mean_bfdu_sr_allo);
                printf("WFDU_SR_ALLO: %f\n", v_stts_algo[i].mean_wfdu_sr_allo);
                printf("FFDU_SR_ALLO: %f\n", v_stts_algo[i].mean_ffdu_sr_allo);
                printf("BFDU_SR_DISP: %f\n", v_stts_algo[i].mean_bfdu_sr_disp);
                printf("WFDU_SR_DISP: %f\n", v_stts_algo[i].mean_wfdu_sr_disp);
                printf("FFDU_SR_DISP: %f\n", v_stts_algo[i].mean_ffdu_sr_disp);
                printf("BFDU_SR_SWAP: %f\n", v_stts_algo[i].mean_bfdu_sr_swap);
                printf("WFDU_SR_SWAP: %f\n", v_stts_algo[i].mean_wfdu_sr_swap);
                printf("FFDU_SR_SWAP: %f\n", v_stts_algo[i].mean_ffdu_sr_swap);
                printf("BFDU_ET:      %f\n", v_stts_algo[i].mean_bfdu_et);
                printf("WFDU_ET:      %f\n", v_stts_algo[i].mean_wfdu_et);
                printf("FFDU_ET:      %f\n", v_stts_algo[i].mean_ffdu_et);

                printf("\n");
        }
}
