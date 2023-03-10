#include "exp.h"
#include "print.h"
#include "generator.h"

#define STEP   2
#define ITER   26
#define SIMNBR 1000

static char const *cmd_gnuplot_cr[] = {};
static char const *cmd_gnuplot_cc[] = {};
static char const *cmd_gnuplot_et[] = {};
static char const *cmd_gnuplot_sr[] = {};

static void phi_nh(vector<struct b_stats> &v_stts_bfdu_f)
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
        prm.n = 100;
        prm.h = NO;
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
                        printf("PHI: %d ITER: %d SIM: %d\n", prm.phi, i, j);

                        /* stats */
                        cmp_stats(v_bins_bfdu_f, v_itms_bfdu_f, ctx_bfdu_f);
                        stts_bfdu_f.mean_cr += ctx_bfdu_f.p.cr;
                        stts_bfdu_f.mean_cc += (float)ctx_bfdu_f.bins_count;
                        stts_bfdu_f.mean_cc_opti += (float)ctx_bfdu_f.bins_min;
                        stts_bfdu_f.mean_cc_allo += (float)(ctx_bfdu_f.cycl_count + ctx_bfdu_f.bins_min);
                        stts_bfdu_f.mean_et += ctx_bfdu_f.p.et * MSEC;
                        stts_bfdu_f.mean_sr_allo += ctx_bfdu_f.p.sched_rate_allo;               
                        stts_bfdu_f.mean_sr_opti += ctx_bfdu_f.p.sched_rate_opti;               
                        stts_bfdu_f.mean_sr_augm += ctx_bfdu_f.p.sched_rate_augm;               
                }
                /* mean */
                stts_bfdu_f.mean_cr /= (float)SIMNBR;
                stts_bfdu_f.mean_cc /= (float)SIMNBR;
                stts_bfdu_f.mean_cc_opti /= (float)SIMNBR;
                stts_bfdu_f.mean_cc_allo /= (float)SIMNBR;
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

        phi_nh(v_stts_bfdu_f);
        print_b_stats(v_stts_bfdu_f, ITER);

        FILE *gnuplot_bfdu_f_cr = (FILE*)popen("gnuplot -persistent", "w");
        FILE *gnuplot_bfdu_f_cc = (FILE*)popen("gnuplot -persistent", "w");
        FILE *gnuplot_bfdu_f_et = (FILE*)popen("gnuplot -persistent", "w");
        FILE *gnuplot_bfdu_f_sr = (FILE*)popen("gnuplot -persistent", "w");

        FILE *bfdu_f_cr = (FILE*)fopen("data.bfdu_f_cr_phi_nh", "w");
        FILE *bfdu_f_cc = (FILE*)fopen("data.bfdu_f_cc_phi_nh", "w");
        FILE *bfdu_f_cc_opti = (FILE*)fopen("data.bfdu_f_cc_opti_phi_nh", "w");
        FILE *bfdu_f_cc_allo = (FILE*)fopen("data.bfdu_f_cc_allo_phi_nh", "w");
        FILE *bfdu_f_et = (FILE*)fopen("data.bfdu_f_et_phi_nh", "w");
        FILE *bfdu_f_sr_allo = (FILE*)fopen("data.bfdu_f_sr_allo_phi_nh", "w");
        FILE *bfdu_f_sr_opti = (FILE*)fopen("data.bfdu_f_sr_opti_phi_nh", "w");
        FILE *bfdu_f_sr_augm = (FILE*)fopen("data.bfdu_f_sr_augm_phi_nh", "w");

        write_data_to_file(bfdu_f_cr, v_stts_bfdu_f, B_CR, ITER);
        write_data_to_file(bfdu_f_cc, v_stts_bfdu_f, B_CC, ITER);
        write_data_to_file(bfdu_f_cc_opti, v_stts_bfdu_f, B_CC_OPTI, ITER);
        write_data_to_file(bfdu_f_cc_allo, v_stts_bfdu_f, B_CC_ALLO, ITER);
        write_data_to_file(bfdu_f_et, v_stts_bfdu_f, B_ET, ITER);
        write_data_to_file(bfdu_f_sr_allo, v_stts_bfdu_f, B_SR_ALLO, ITER);
        write_data_to_file(bfdu_f_sr_opti, v_stts_bfdu_f, B_SR_OPTI, ITER);
        write_data_to_file(bfdu_f_sr_augm, v_stts_bfdu_f, B_SR_AUGM, ITER);

        cmd_gnuplot_cr[0] = "set title 'Cores Ratio NH'";
        cmd_gnuplot_cr[1] = "set xrange [45:105]";
        cmd_gnuplot_cr[2] = "set yrange [1:1.7]";
        cmd_gnuplot_cr[3] = "set xlabel 'phi'";
        cmd_gnuplot_cr[4] = "set ylabel 'M/M*'";
        cmd_gnuplot_cr[5] = "set datafile separator whitespace";
        cmd_gnuplot_cr[6] = "plot 'data.bfdu_f_cr_phi_nh' with linespoint lc 7 pointtype 7";
        plot_data(gnuplot_bfdu_f_cr, cmd_gnuplot_cr, 7);

        cmd_gnuplot_cc[0] = "set title 'Cores Count NH'";
        cmd_gnuplot_cc[1] = "set xrange [45:105]";
        cmd_gnuplot_cc[2] = "set yrange [0:90]";
        cmd_gnuplot_cc[3] = "set xlabel 'phi'";
        cmd_gnuplot_cc[4] = "set ylabel 'cores'";
        cmd_gnuplot_cc[5] = "set datafile separator whitespace";
        cmd_gnuplot_cc[6] = "plot 'data.bfdu_f_cc_phi_nh' with linespoint lc 7 pointtype 7";
        cmd_gnuplot_cc[7] = "replot 'data.bfdu_f_cc_opti_phi_nh' with linespoint lc 3 pointtype 7";
        cmd_gnuplot_cc[8] = "replot 'data.bfdu_f_cc_allo_phi_nh' with linespoint lc 6 pointtype 7";
        plot_data(gnuplot_bfdu_f_cc, cmd_gnuplot_cc, 9);

        cmd_gnuplot_et[0] = "set title 'Execution Time NH (ms)'";
        cmd_gnuplot_et[1] = "set xrange [45:105]";
        cmd_gnuplot_et[2] = "set yrange [0:600]";
        cmd_gnuplot_et[3] = "set xlabel 'phi'";
        cmd_gnuplot_et[4] = "set ylabel 'milliseconds'";
        cmd_gnuplot_et[5] = "set datafile separator whitespace";
        cmd_gnuplot_et[6] = "plot 'data.bfdu_f_et_phi_nh' with linespoint lc 7 pointtype 7";
        plot_data(gnuplot_bfdu_f_et, cmd_gnuplot_et, 7);

        cmd_gnuplot_sr[0] = "set title 'Schedulability Rate NH'";
        cmd_gnuplot_sr[1] = "set xrange [45:105]";
        cmd_gnuplot_sr[2] = "set yrange [0:120]";
        cmd_gnuplot_sr[3] = "set xlabel 'phi'";
        cmd_gnuplot_sr[4] = "set ylabel 'rate'";
        cmd_gnuplot_sr[5] = "set datafile separator whitespace";
        cmd_gnuplot_sr[6] = "plot 'data.bfdu_f_sr_augm_phi_nh' with linespoint lc 7 pointtype 7";
        cmd_gnuplot_sr[7] = "replot 'data.bfdu_f_sr_allo_phi_nh' with linespoint lc 3 pointtype 7";
        cmd_gnuplot_sr[8] = "replot 'data.bfdu_f_sr_opti_phi_nh' with linespoint lc 6 pointtype 7";
        plot_data(gnuplot_bfdu_f_sr, cmd_gnuplot_sr, 9);

        fflush(gnuplot_bfdu_f_cr);
        fflush(gnuplot_bfdu_f_cc);
        fflush(gnuplot_bfdu_f_et);
        fflush(gnuplot_bfdu_f_sr);

        fclose(bfdu_f_cr);
        fclose(bfdu_f_cc);
        fclose(bfdu_f_et);
        fclose(bfdu_f_cc_opti);
        fclose(bfdu_f_cc_allo);
        fclose(bfdu_f_sr_allo);
        fclose(bfdu_f_sr_opti);
        fclose(bfdu_f_sr_augm);

        return 0;
}
