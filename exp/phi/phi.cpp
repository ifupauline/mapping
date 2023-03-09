#include "exp.h"
#include "print.h"
#include "generator.h"

#define STEP   2
#define ITER   26
#define SIMNBR 100

static char const *cmd_gnuplot_cr[] = {};
static char const *cmd_gnuplot_et[] = {};
static char const *cmd_gnuplot_sr[] = {};

static void phi(vector<struct b_stats> &v_stts_bfdu_f)
{
        struct params prm;
        struct context ctx;
        struct context ctx_bfdu_f;
        struct b_stats stts_bfdu_f;
        vector<struct bin> v_bins;
        vector<struct item> v_itms;
        vector<struct item> v_itms_bfdu_f;
        vector<struct bin> v_bins_bfdu_f;

        prm = {0};
        ctx = {0};

        /* params instance */
        prm.n = 50;
        prm.phi = 102;

        for (int i = 0; i < ITER; i++) {
                prm.phi -= STEP;
                stts_bfdu_f = {0};
                stts_bfdu_f.phi = prm.phi;

                for (int j = 0; j < SIMNBR; j++) {
                        v_itms.clear();
                        v_bins.clear();
                        v_itms_bfdu_f.clear();
                        v_bins_bfdu_f.clear();

                        gen_tc_set(v_itms, prm, ctx);
                        init_ctx(v_itms, prm, ctx);

                        ctx_bfdu_f = {0};

                        ctx_bfdu_f = ctx;
                        ctx_bfdu_f.prm.a = BFDU_F;

                        v_itms_bfdu_f = v_itms;
                        v_bins_bfdu_f = v_bins;

                        generation(v_bins_bfdu_f, ctx_bfdu_f);

                        allocation(v_itms_bfdu_f, v_bins_bfdu_f, ctx_bfdu_f);

                        schedulability_analysis(v_bins_bfdu_f, ctx_bfdu_f);

                        optimization(v_bins_bfdu_f, ctx_bfdu_f);

                        augmentation(v_bins_bfdu_f, ctx_bfdu_f);

                        /* stats */
                        cmp_stats(v_bins_bfdu_f, v_itms_bfdu_f, ctx_bfdu_f);
                        stts_bfdu_f.mean_cr += ctx_bfdu_f.p.cr;
                        stts_bfdu_f.mean_et += ctx_bfdu_f.p.et * 1000;
                        stts_bfdu_f.mean_sr_allo += ctx_bfdu_f.p.sched_rate_allo;               
                        stts_bfdu_f.mean_sr_opti += ctx_bfdu_f.p.sched_rate_opti;               
                        stts_bfdu_f.mean_sr_augm += ctx_bfdu_f.p.sched_rate_augm;               
                }
                /* mean */
                stts_bfdu_f.mean_cr /= (float)SIMNBR;
                stts_bfdu_f.mean_et /= (float)SIMNBR;
                stts_bfdu_f.mean_sr_allo /= (float)SIMNBR;
                stts_bfdu_f.mean_sr_opti /= (float)SIMNBR;
                stts_bfdu_f.mean_sr_augm /= (float)SIMNBR;
                v_stts_bfdu_f.push_back(stts_bfdu_f);
        }
}

int main(void)
{
        vector<struct b_stats> v_stts_bfdu_f;

        phi(v_stts_bfdu_f);
        print_b_stats(v_stts_bfdu_f, ITER);

        FILE *gnuplot_bfdu_f_cr = (FILE*)popen("gnuplot -persistent", "w");
        FILE *gnuplot_bfdu_f_et = (FILE*)popen("gnuplot -persistent", "w");
        FILE *gnuplot_bfdu_f_sr = (FILE*)popen("gnuplot -persistent", "w");

        FILE *bfdu_f_cr = (FILE*)fopen("data.bfdu_f_cr_phi", "w");
        FILE *bfdu_f_et = (FILE*)fopen("data.bfdu_f_et_phi", "w");
        FILE *bfdu_f_sr_allo = (FILE*)fopen("data.bfdu_f_sr_allo_phi", "w");
        FILE *bfdu_f_sr_opti = (FILE*)fopen("data.bfdu_f_sr_opti_phi", "w");
        FILE *bfdu_f_sr_augm = (FILE*)fopen("data.bfdu_f_sr_augm_phi", "w");

        write_data_to_file(bfdu_f_cr, v_stts_bfdu_f, B_CR, ITER);
        write_data_to_file(bfdu_f_et, v_stts_bfdu_f, B_ET, ITER);
        write_data_to_file(bfdu_f_sr_allo, v_stts_bfdu_f, B_SR_ALLO, ITER);
        write_data_to_file(bfdu_f_sr_opti, v_stts_bfdu_f, B_SR_OPTI, ITER);
        write_data_to_file(bfdu_f_sr_augm, v_stts_bfdu_f, B_SR_AUGM, ITER);

        cmd_gnuplot_cr[0] = "set title 'Cores Ratio'";
        cmd_gnuplot_cr[1] = "set xrange [45:105]";
        cmd_gnuplot_cr[2] = "set yrange [1:1.8]";
        cmd_gnuplot_cr[3] = "set xlabel 'phi'";
        cmd_gnuplot_cr[4] = "set ylabel 'M/M*'";
        cmd_gnuplot_cr[5] = "set datafile separator whitespace";
        cmd_gnuplot_cr[6] = "plot 'data.bfdu_f_cr_phi' with linespoint lc 7 pointtype 7";
        plot_data(gnuplot_bfdu_f_cr, cmd_gnuplot_cr, 7);

        cmd_gnuplot_et[0] = "set title 'Execution Time (ms)'";
        cmd_gnuplot_et[1] = "set xrange [45:105]";
        cmd_gnuplot_et[2] = "set yrange [0:60]";
        cmd_gnuplot_et[3] = "set xlabel 'phi'";
        cmd_gnuplot_et[4] = "set ylabel 'milliseconds'";
        cmd_gnuplot_et[5] = "set datafile separator whitespace";
        cmd_gnuplot_et[6] = "plot 'data.bfdu_f_et_phi' with linespoint lc 7 pointtype 7";
        plot_data(gnuplot_bfdu_f_et, cmd_gnuplot_et, 7);

        cmd_gnuplot_sr[0] = "set title 'Schedulability Rate'";
        cmd_gnuplot_sr[1] = "set xrange [45:105]";
        cmd_gnuplot_sr[2] = "set yrange [0:120]";
        cmd_gnuplot_sr[3] = "set xlabel 'phi'";
        cmd_gnuplot_sr[4] = "set ylabel 'rate'";
        cmd_gnuplot_sr[5] = "set datafile separator whitespace";
        cmd_gnuplot_sr[6] = "plot 'data.bfdu_f_sr_augm_phi' with linespoint lc 7 pointtype 7";
        cmd_gnuplot_sr[7] = "replot 'data.bfdu_f_sr_allo_phi' with linespoint lc 3 pointtype 7";
        cmd_gnuplot_sr[8] = "replot 'data.bfdu_f_sr_opti_phi' with linespoint lc 6 pointtype 7";
        plot_data(gnuplot_bfdu_f_sr, cmd_gnuplot_sr, 9);

        return 0;
}
