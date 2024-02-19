#include "exp.h"

void write_to_file(FILE *filename, vector<float> &v_float)
{
        for (unsigned int i = 0; i < v_float.size(); i++) {
                fprintf(filename, "%-3.2f", v_float[i]);
                if (i == v_float.size() - 1) {
                        fprintf(filename, "\n");
                        break;
                }
                fprintf(filename, ",");
        }
}

void write_data_to_file(FILE *filename, vector<struct b_stats> &data,
                int type, int size)
{
        switch (type) {
                case B_M: 
                        for (int i = 0; i < size; i++)
                                fprintf(filename, "%-3.2f, %-3.2f, %-3.2f, %-3.2f\n", 
                                                data[i].sig, 
                                                data[i].bfdu_m,
                                                data[i].wfdu_m,
                                                data[i].ffdu_m);
                        break;

                case B_SR_ALLO: 
                        for (int i = 0; i < size; i++)
                                fprintf(filename, "%-3.2f, %-3.2f, %-3.2f, %-3.2f\n", 
                                                data[i].sig, 
                                                data[i].bfdu_sr_allo,
                                                data[i].wfdu_sr_allo,
                                                data[i].ffdu_sr_allo);
                        break;

                case B_SR_DISP: 
                        for (int i = 0; i < size; i++)
                                fprintf(filename, "%-3.2f, %-3.2f, %-3.2f, %-3.2f\n", 
                                                data[i].sig, 
                                                data[i].bfdu_sr_disp,
                                                data[i].wfdu_sr_disp,
                                                data[i].ffdu_sr_disp);
                        break;

                case B_SR_SWAP: 
                        for (int i = 0; i < size; i++)
                                fprintf(filename, "%-3.2f, %-3.2f, %-3.2f, %-3.2f\n", 
                                                data[i].sig, 
                                                data[i].bfdu_sr_swap,
                                                data[i].wfdu_sr_swap,
                                                data[i].ffdu_sr_swap);
                        break;

                case B_ET: 
                        for (int i = 0; i < size; i++)
                                fprintf(filename, "%-3.2f, %-3.2f, %-3.2f, %-3.2f\n", 
                                                data[i].sig, 
                                                data[i].bfdu_et,
                                                data[i].wfdu_et,
                                                data[i].ffdu_et);
                        break;
                default:
                        printf("Error! Unknown type of data\n");
        }
}

void print_b_stats(vector<struct b_stats> &v_stts_algo, int iter)
{
        for (int i = 0; i < iter; i++) {
                printf("+===============================+\n");
                printf("| SIG: %.2lf                     |\n", v_stts_algo[i].sig);
                printf("+===============================+\n");
                printf("|BFDU_M:       %f\n", v_stts_algo[i].bfdu_m);
                printf("|WFDU_M:       %f\n", v_stts_algo[i].wfdu_m);
                printf("|FFDU_M:       %f\n", v_stts_algo[i].ffdu_m);
                printf("+-------------------------------|\n");
                printf("|BFDU_SR_ALLO: %f\n", v_stts_algo[i].bfdu_sr_allo);
                printf("|WFDU_SR_ALLO: %f\n", v_stts_algo[i].wfdu_sr_allo);
                printf("|FFDU_SR_ALLO: %f\n", v_stts_algo[i].ffdu_sr_allo);
                printf("+-------------------------------|\n");
                printf("|BFDU_SR_DISP: %f\n", v_stts_algo[i].bfdu_sr_disp);
                printf("|WFDU_SR_DISP: %f\n", v_stts_algo[i].wfdu_sr_disp);
                printf("|FFDU_SR_DISP: %f\n", v_stts_algo[i].ffdu_sr_disp);
                printf("+-------------------------------|\n");
                printf("|BFDU_SR_SWAP: %f\n", v_stts_algo[i].bfdu_sr_swap);
                printf("|WFDU_SR_SWAP: %f\n", v_stts_algo[i].wfdu_sr_swap);
                printf("|FFDU_SR_SWAP: %f\n", v_stts_algo[i].ffdu_sr_swap);
                printf("+-------------------------------|\n");
                printf("|BFDU_ET:      %f\n", v_stts_algo[i].bfdu_et);
                printf("|WFDU_ET:      %f\n", v_stts_algo[i].wfdu_et);
                printf("|FFDU_ET:      %f\n", v_stts_algo[i].ffdu_et);
                printf("+-------------------------------|\n\n");
        }
}
