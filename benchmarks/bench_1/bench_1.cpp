#include "print.h"
#include "bench.h"
#include "generator.h"

#define STEP   1
#define ITER   50
#define SIMNBR 300

static char const *cmd_gnuplot_ar[] = {};
static char const *cmd_gnuplot_et[] = {};
static char const *cmd_gnuplot_ld[] = {};
static char const *cmd_gnuplot_sched[] = {};

static void bench_1(vector<struct b_stats> &v_stts_bfdu_f, 
                vector<struct b_stats> &v_stts_wfdu_f)
{
        struct params prm;

        /* params instance */
        prm.n = 100;
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

                        gen_tc_set(v_itms, prm, ctx);
                        init_ctx(v_itms, prm, ctx);

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

                        schedulability_analysis(v_bins_bfdu_f, ctx_bfdu_f);
                        schedulability_analysis(v_bins_wfdu_f, ctx_wfdu_f);

                        /* stats */
                        cmp_stats(v_bins_bfdu_f, v_itms_bfdu_f, ctx_bfdu_f);
                        cmp_stats(v_bins_wfdu_f, v_itms_wfdu_f, ctx_wfdu_f);
                        stts_bfdu_f.mean_ar += ctx_bfdu_f.p.opti_bins;
                        stts_wfdu_f.mean_ar += ctx_wfdu_f.p.opti_bins;
                        stts_bfdu_f.mean_ld += ctx_bfdu_f.p.standard_dev;
                        stts_wfdu_f.mean_ld += ctx_wfdu_f.p.standard_dev;
                        stts_bfdu_f.mean_et += ctx_bfdu_f.p.e_time;
                        stts_wfdu_f.mean_et += ctx_wfdu_f.p.e_time;               
                        stts_bfdu_f.mean_sched += ctx_bfdu_f.p.sched_rate_aft * PERCENT;
                        stts_wfdu_f.mean_sched += ctx_wfdu_f.p.sched_rate_aft * PERCENT;
                }
                /* mean */
                stts_bfdu_f.mean_ar /= (float)SIMNBR;
                stts_wfdu_f.mean_ar /= (float)SIMNBR;
                stts_bfdu_f.mean_et /= (float)SIMNBR;
                stts_wfdu_f.mean_et /= (float)SIMNBR;
                stts_bfdu_f.mean_ld /= (float)SIMNBR;
                stts_wfdu_f.mean_ld /= (float)SIMNBR;
                stts_bfdu_f.mean_sched /= (float)SIMNBR;
                stts_wfdu_f.mean_sched /= (float)SIMNBR;
                v_stts_bfdu_f.push_back(stts_bfdu_f);
                v_stts_wfdu_f.push_back(stts_wfdu_f);
        }
}

int main(void)
{
        vector<struct b_stats> v_stts_bfdu_f;
        vector<struct b_stats> v_stts_wfdu_f;

        bench_1(v_stts_bfdu_f, v_stts_wfdu_f);
        print_b_stats(v_stts_bfdu_f, v_stts_wfdu_f, ITER);

        FILE *gnuplot_bfdu_f_ar = (FILE*)popen("gnuplot -persistent", "w");
        FILE *gnuplot_wfdu_f_ar = (FILE*)popen("gnuplot -persistent", "w");
        FILE *gnuplot_bfdu_f_et = (FILE*)popen("gnuplot -persistent", "w");
        FILE *gnuplot_wfdu_f_et = (FILE*)popen("gnuplot -persistent", "w");
        FILE *gnuplot_bfdu_f_ld = (FILE*)popen("gnuplot -persistent", "w");
        FILE *gnuplot_wfdu_f_ld = (FILE*)popen("gnuplot -persistent", "w");
        FILE *gnuplot_bfdu_f_sched = (FILE*)popen("gnuplot -persistent", "w");
        FILE *gnuplot_wfdu_f_sched = (FILE*)popen("gnuplot -persistent", "w");

        FILE *bfdu_f_ar = (FILE*)fopen("data.bfdu_f_ar_bench_1", "w");
        FILE *wfdu_f_ar = (FILE*)fopen("data.wfdu_f_ar_bench_1", "w");
        FILE *bfdu_f_et = (FILE*)fopen("data.bfdu_f_et_bench_1", "w");
        FILE *wfdu_f_et = (FILE*)fopen("data.wfdu_f_et_bench_1", "w");
        FILE *bfdu_f_ld = (FILE*)fopen("data.bfdu_f_ld_bench_1", "w");
        FILE *wfdu_f_ld = (FILE*)fopen("data.wfdu_f_ld_bench_1", "w");
        FILE *bfdu_f_sched = (FILE*)fopen("data.bfdu_f_sched_bench_1", "w");
        FILE *wfdu_f_sched = (FILE*)fopen("data.wfdu_f_sched_bench_1", "w");

        write_data_to_file(bfdu_f_ar, v_stts_bfdu_f, B_AR, ITER);
        write_data_to_file(wfdu_f_ar, v_stts_wfdu_f, B_AR, ITER);
        write_data_to_file(bfdu_f_et, v_stts_bfdu_f, B_ET, ITER);
        write_data_to_file(wfdu_f_et, v_stts_wfdu_f, B_ET, ITER);
        write_data_to_file(bfdu_f_ld, v_stts_bfdu_f, B_LD, ITER);
        write_data_to_file(wfdu_f_ld, v_stts_wfdu_f, B_LD, ITER);
        write_data_to_file(bfdu_f_sched, v_stts_bfdu_f, B_SCHED, ITER);
        write_data_to_file(wfdu_f_sched, v_stts_wfdu_f, B_SCHED, ITER);

        cmd_gnuplot_ar[0] = "set title 'Approximation Ratio (Bins)'";
        cmd_gnuplot_ar[1] = "set xrange [100:45]";
        cmd_gnuplot_ar[2] = "set yrange [1:1.3]";
        cmd_gnuplot_ar[3] = "set xlabel 'phi'";
        cmd_gnuplot_ar[4] = "set ylabel 'approximation ratio'";
        cmd_gnuplot_ar[5] = "set datafile separator whitespace";
        cmd_gnuplot_ar[6] = "plot 'data.bfdu_f_ar_bench_1' with linespoint lc 22 pointtype 22";
        cmd_gnuplot_ar[7] = "set datafile separator whitespace";
        cmd_gnuplot_ar[8] = "replot 'data.wfdu_f_ar_bench_1' with linespoint lc 7 pointtype 7";
        plot_data(gnuplot_bfdu_f_ar, cmd_gnuplot_ar, 9);

        cmd_gnuplot_et[0] = "set title 'Execution Time (ms)'";
        cmd_gnuplot_et[1] = "set xrange [100:45]";
        cmd_gnuplot_et[2] = "set yrange [0:2]";
        cmd_gnuplot_et[3] = "set xlabel 'phi'";
        cmd_gnuplot_et[4] = "set ylabel 'time (ms)'";
        cmd_gnuplot_et[5] = "set datafile separator whitespace";
        cmd_gnuplot_et[6] = "plot 'data.bfdu_f_et_bench_1' with linespoint lc 22 pointtype 22";
        cmd_gnuplot_et[7] = "set datafile separator whitespace";
        cmd_gnuplot_et[8] = "replot 'data.wfdu_f_et_bench_1' with linespoint lc 7 pointtype 7";
        plot_data(gnuplot_bfdu_f_et, cmd_gnuplot_et, 9);

        cmd_gnuplot_ld[0] = "set title 'Load Distribution'";
        cmd_gnuplot_ld[1] = "set xrange [100:45]";
        cmd_gnuplot_ld[2] = "set yrange [0:20]";
        cmd_gnuplot_ld[3] = "set xlabel 'phi'";
        cmd_gnuplot_ld[4] = "set ylabel 'standard deviation on U'";
        cmd_gnuplot_ld[5] = "set datafile separator whitespace";
        cmd_gnuplot_ld[6] = "plot 'data.bfdu_f_ld_bench_1' with linespoint lc 22 pointtype 22";
        cmd_gnuplot_ld[7] = "set datafile separator whitespace";
        cmd_gnuplot_ld[8] = "replot 'data.wfdu_f_ld_bench_1' with linespoint lc 7 pointtype 7";
        plot_data(gnuplot_bfdu_f_ld, cmd_gnuplot_ld, 9);

        cmd_gnuplot_sched[0] = "set title 'Schedulability Rate'";
        cmd_gnuplot_sched[1] = "set xrange [100:45]";
        cmd_gnuplot_sched[2] = "set yrange [0:120]";
        cmd_gnuplot_sched[3] = "set xlabel 'phi'";
        cmd_gnuplot_sched[4] = "set ylabel 'schedulability rate'";
        cmd_gnuplot_sched[5] = "set datafile separator whitespace";
        cmd_gnuplot_sched[6] = "plot 'data.bfdu_f_sched_bench_1' with linespoint lc 22 pointtype 22";
        cmd_gnuplot_sched[7] = "set datafile separator whitespace";
        cmd_gnuplot_sched[8] = "replot 'data.wfdu_f_sched_bench_1' with linespoint lc 7 pointtype 7";
        plot_data(gnuplot_bfdu_f_sched, cmd_gnuplot_sched, 9);

        fflush(gnuplot_bfdu_f_ar);
        fflush(gnuplot_bfdu_f_et);
        fflush(gnuplot_bfdu_f_ld);
        fflush(gnuplot_bfdu_f_sched);
        fclose(gnuplot_wfdu_f_ar);
        fclose(gnuplot_wfdu_f_et);
        fclose(gnuplot_wfdu_f_ld);
        fclose(gnuplot_wfdu_f_sched);

        return 0;
}
