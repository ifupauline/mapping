#include "bench.h"
#include "generator.h"

#define STEP     2
#define ITER     25
#define SIMNBR   500

static char const *cmd_gnuplot_ar[] = {};
static char const *cmd_gnuplot_et[] = {};
static char const *cmd_gnuplot_ld[] = {};

static void k_500(vector<struct b_stats> &v_stts_bfdu_f, 
                vector<struct b_stats> &v_stts_wfdu_f)
{
        struct params prm;

        /* params instance */
        prm.n = 10;
        prm.s = 100;
        prm.c = 100;
        prm.phi = 100;

        for (int i = 0; i < ITER; i++) {
                struct b_stats stts_bfdu_f;
                struct b_stats stts_wfdu_f;

                prm.phi -= STEP;
                stts_bfdu_f = {0};
                stts_wfdu_f = {0};
                stts_bfdu_f.phi = prm.phi;
                stts_wfdu_f.phi = prm.phi;

                for (int j = 0; j < SIMNBR; j++) {
                        struct context ctx;
                        vector<struct item> v_itms;
                        vector<struct bin> v_bins;

                        gen_tc_set(v_itms, prm);
                        init_ctx(prm, ctx);
                        comp_min_bins(v_itms, ctx);

                        struct context ctx_bfdu_f = ctx;
                        struct context ctx_wfdu_f = ctx;
                        ctx_bfdu_f.prm.a = BFDU_F;
                        ctx_wfdu_f.prm.a = WFDU_F;

                        vector<struct item> v_itms_bfdu_f = v_itms;
                        vector<struct item> v_itms_wfdu_f = v_itms;
                        vector<struct bin> v_bins_bfdu_f = v_bins;
                        vector<struct bin> v_bins_wfdu_f = v_bins;

                        generation(v_bins_bfdu_f, ctx_bfdu_f);
                        generation(v_bins_wfdu_f, ctx_wfdu_f);

                        reduction(v_itms_bfdu_f, v_bins_bfdu_f, ctx_bfdu_f);
                        reduction(v_itms_wfdu_f, v_bins_wfdu_f, ctx_wfdu_f);

                        allocation(v_itms_bfdu_f, v_bins_bfdu_f, ctx_bfdu_f);
                        allocation(v_itms_wfdu_f, v_bins_wfdu_f, ctx_wfdu_f);

                        /* stats */
                        comp_stats(v_bins_bfdu_f, v_itms_bfdu_f, ctx_bfdu_f);
                        comp_stats(v_bins_wfdu_f, v_itms_wfdu_f, ctx_wfdu_f);
                        stts_bfdu_f.mean_ar += ctx_bfdu_f.opti_bins;
                        stts_wfdu_f.mean_ar += ctx_wfdu_f.opti_bins;
                        stts_bfdu_f.mean_ld += ctx_bfdu_f.standard_dev;
                        stts_wfdu_f.mean_ld += ctx_wfdu_f.standard_dev;
                        stts_bfdu_f.mean_et += ctx_bfdu_f.e_time;
                        stts_wfdu_f.mean_et += ctx_wfdu_f.e_time;               
                }
                /* mean */
                stts_bfdu_f.mean_ar /= (float)SIMNBR;
                stts_wfdu_f.mean_ar /= (float)SIMNBR;
                stts_bfdu_f.mean_et /= (float)SIMNBR;
                stts_wfdu_f.mean_et /= (float)SIMNBR;
                stts_bfdu_f.mean_ld /= (float)SIMNBR;
                stts_wfdu_f.mean_ld /= (float)SIMNBR;
                v_stts_bfdu_f.push_back(stts_bfdu_f);
                v_stts_wfdu_f.push_back(stts_wfdu_f);
        }
}

int main(void)
{
        vector<struct b_stats> v_stts_bfdu_f;
        vector<struct b_stats> v_stts_wfdu_f;

        k_500(v_stts_bfdu_f, v_stts_wfdu_f);
        print_b_stats(v_stts_bfdu_f, v_stts_wfdu_f, ITER);

        FILE *gnuplot_bfdu_f_ar = (FILE*)popen("gnuplot -persistent", "w");
        FILE *gnuplot_wfdu_f_ar = (FILE*)popen("gnuplot -persistent", "w");
        FILE *gnuplot_bfdu_f_et = (FILE*)popen("gnuplot -persistent", "w");
        FILE *gnuplot_wfdu_f_et = (FILE*)popen("gnuplot -persistent", "w");
        FILE *gnuplot_bfdu_f_ld = (FILE*)popen("gnuplot -persistent", "w");
        FILE *gnuplot_wfdu_f_ld = (FILE*)popen("gnuplot -persistent", "w");

        FILE *bfdu_f_ar = (FILE*)fopen("data.bfdu_f_ar_k500", "w");
        FILE *wfdu_f_ar = (FILE*)fopen("data.wfdu_f_ar_k500", "w");
        FILE *bfdu_f_et = (FILE*)fopen("data.bfdu_f_et_k500", "w");
        FILE *wfdu_f_et = (FILE*)fopen("data.wfdu_f_et_k500", "w");
        FILE *bfdu_f_ld = (FILE*)fopen("data.bfdu_f_ld_k500", "w");
        FILE *wfdu_f_ld = (FILE*)fopen("data.wfdu_f_ld_k500", "w");

        write_data_to_file(bfdu_f_ar, v_stts_bfdu_f, B_AR, ITER);
        write_data_to_file(wfdu_f_ar, v_stts_wfdu_f, B_AR, ITER);
        write_data_to_file(bfdu_f_et, v_stts_bfdu_f, B_ET, ITER);
        write_data_to_file(wfdu_f_et, v_stts_wfdu_f, B_ET, ITER);
        write_data_to_file(bfdu_f_ld, v_stts_bfdu_f, B_LD, ITER);
        write_data_to_file(wfdu_f_ld, v_stts_wfdu_f, B_LD, ITER);

        cmd_gnuplot_ar[0] = "set title 'Approximation Ratio (Bins)'";
        cmd_gnuplot_ar[1] = "set xrange [100:45]";
        cmd_gnuplot_ar[2] = "set yrange [1:1.3]";
        cmd_gnuplot_ar[3] = "set xlabel 'phi'";
        cmd_gnuplot_ar[4] = "set ylabel 'approximation ratio'";
        cmd_gnuplot_ar[5] = "plot 'data.bfdu_f_ar_k500' with linespoint lc 22 pointtype 22";
        cmd_gnuplot_ar[6] = "replot 'data.wfdu_f_ar_k500' with linespoint lc 7 pointtype 7";
        plot_data(gnuplot_bfdu_f_ar, cmd_gnuplot_ar, 7);

        cmd_gnuplot_et[0] = "set title 'Execution Time (ms)'";
        cmd_gnuplot_et[1] = "set xrange [100:45]";
        cmd_gnuplot_et[2] = "set yrange [0:22]";
        cmd_gnuplot_et[3] = "set xlabel 'phi'";
        cmd_gnuplot_et[4] = "set ylabel 'time (ms)'";
        cmd_gnuplot_et[5] = "plot 'data.bfdu_f_et_k500' with linespoint lc 22 pointtype 22";
        cmd_gnuplot_et[6] = "replot 'data.wfdu_f_et_k500' with linespoint lc 7 pointtype 7";
        plot_data(gnuplot_bfdu_f_et, cmd_gnuplot_et, 7);

        cmd_gnuplot_ld[0] = "set title 'Load Distribution'";
        cmd_gnuplot_ld[1] = "set xrange [100:45]";
        cmd_gnuplot_ld[2] = "set yrange [0:20]";
        cmd_gnuplot_ld[3] = "set xlabel 'phi'";
        cmd_gnuplot_ld[4] = "set ylabel 'standard deviation on U'";
        cmd_gnuplot_ld[5] = "plot 'data.bfdu_f_ld_k500' with linespoint lc 22 pointtype 22";
        cmd_gnuplot_ld[6] = "replot 'data.wfdu_f_ld_k500' with linespoint lc 7 pointtype 7";
        plot_data(gnuplot_bfdu_f_ld, cmd_gnuplot_ld, 7);

        fflush(gnuplot_bfdu_f_ar);
        fflush(gnuplot_bfdu_f_et);
        fflush(gnuplot_bfdu_f_ld);
        fclose(gnuplot_wfdu_f_ar);
        fclose(gnuplot_wfdu_f_et);
        fclose(gnuplot_wfdu_f_ld);

        return 0;
}
