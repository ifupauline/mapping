#include "exp.h"
#include "print.h"
#include "generator.h"

#define STEP   20
#define ITER   25
#define SIMNBR 1000

static char const *cmd_gnuplot_cr[] = {};
static char const *cmd_gnuplot_et[] = {};
static char const *cmd_gnuplot_sr[] = {};
static char const *cmd_gnuplot_fr[] = {};
static char const *cmd_gnuplot_op[] = {};

static void _cmp_avr_mean(vector<struct b_stats> &v_stts_bfdu_f)
{
        float sum_reas;
        float sum_disp;
        float sum_swap;

        sum_reas = 0.0;
        sum_disp = 0.0;
        sum_swap = 0.0;

        for (unsigned int i = 0; i < v_stts_bfdu_f.size(); i++) {
                sum_reas += v_stts_bfdu_f[i].mean_reas;
                sum_disp += v_stts_bfdu_f[i].mean_disp;
                sum_swap += v_stts_bfdu_f[i].mean_swap;
        }
        v_stts_bfdu_f[0].total_mean_reas = sum_reas / ITER;
        v_stts_bfdu_f[0].total_mean_disp = sum_disp / ITER;
        v_stts_bfdu_f[0].total_mean_swap = sum_swap / ITER;
}

static void phi(vector<struct b_stats> &v_stts_bfdu_f, struct params &prm)
{
        struct context ctx;
        struct context ctx_bfdu_f;
        struct b_stats stts_bfdu_f;
        vector<struct bin> v_bins;
        vector<struct item> v_itms;
        vector<struct item> v_itms_bfdu_f;
        vector<struct bin> v_bins_bfdu_f;

        ctx = {0};

        /* params instance */
        prm.n = 100;
        prm.phi = 1000;

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

                        printf("PHI: %d ITER: %d SIM: %d\n", prm.phi, ITER - i, j);

                        /* stats */
                        cmp_stats(v_bins_bfdu_f, v_itms_bfdu_f, ctx_bfdu_f);
                        stts_bfdu_f.mean_cr += ctx_bfdu_f.p.cr;
                        stts_bfdu_f.mean_fr += ctx_bfdu_f.p.fr;
                        stts_bfdu_f.mean_et += ctx_bfdu_f.p.et * MSEC;
                        stts_bfdu_f.mean_sr_allo += ctx_bfdu_f.p.sched_rate_allo;               
                        stts_bfdu_f.mean_sr_opti += ctx_bfdu_f.p.sched_rate_opti;               
                        stts_bfdu_f.mean_reas += ctx_bfdu_f.p.reas_gain;
                        stts_bfdu_f.mean_disp += ctx_bfdu_f.p.disp_gain;
                        stts_bfdu_f.mean_swap += ctx_bfdu_f.p.swap_gain;
                }
                /* mean */
                stts_bfdu_f.mean_cr /= (float)SIMNBR;
                stts_bfdu_f.mean_fr /= (float)SIMNBR;
                stts_bfdu_f.mean_et /= (float)SIMNBR;
                stts_bfdu_f.mean_sr_allo /= (float)SIMNBR;
                stts_bfdu_f.mean_sr_opti /= (float)SIMNBR;
                stts_bfdu_f.mean_reas /= (float)SIMNBR;
                stts_bfdu_f.mean_disp /= (float)SIMNBR;
                stts_bfdu_f.mean_swap /= (float)SIMNBR;
                v_stts_bfdu_f.push_back(stts_bfdu_f);
        }
        _cmp_avr_mean(v_stts_bfdu_f);
}

int main(int argc, char **argv)
{
        if (argc != 2) {
                printf("Wrong Number of Parameters -> ./phi 0 (non-harmonics task-chain) 1 (harmonics task-chain)\n");
                exit(0);
        }
        struct params prm;
        vector<struct b_stats> v_stts_bfdu_f;

        prm = {0};
        prm.h = atoi(argv[1]);

        phi(v_stts_bfdu_f, prm);
        print_b_stats(v_stts_bfdu_f, ITER);

        FILE *gnuplot_bfdu_f_cr = (FILE*)popen("gnuplot -persistent", "w");
        FILE *gnuplot_bfdu_f_fr = (FILE*)popen("gnuplot -persistent", "w");
        FILE *gnuplot_bfdu_f_et = (FILE*)popen("gnuplot -persistent", "w");
        FILE *gnuplot_bfdu_f_sr = (FILE*)popen("gnuplot -persistent", "w");
        FILE *gnuplot_bfdu_f_op = (FILE*)popen("gnuplot -persistent", "w");

        FILE *bfdu_f_cr = (FILE*)fopen("data.bfdu_f_cr_phi", "w");
        FILE *bfdu_f_fr = (FILE*)fopen("data.bfdu_f_fr_phi", "w");
        FILE *bfdu_f_et = (FILE*)fopen("data.bfdu_f_et_phi", "w");
        FILE *bfdu_f_op = (FILE*)fopen("data.bfdu_f_op_phi", "w");
        FILE *bfdu_f_sr_allo = (FILE*)fopen("data.bfdu_f_sr_allo_phi", "w");
        FILE *bfdu_f_sr_opti = (FILE*)fopen("data.bfdu_f_sr_opti_phi", "w");

        write_data_to_file(bfdu_f_cr, v_stts_bfdu_f, B_CR, ITER);
        write_data_to_file(bfdu_f_fr, v_stts_bfdu_f, B_FR, ITER);
        write_data_to_file(bfdu_f_et, v_stts_bfdu_f, B_ET, ITER);
        write_data_to_file(bfdu_f_op, v_stts_bfdu_f, B_OPT, ITER);
        write_data_to_file(bfdu_f_sr_allo, v_stts_bfdu_f, B_SR_ALLO, ITER);
        write_data_to_file(bfdu_f_sr_opti, v_stts_bfdu_f, B_SR_OPTI, ITER);

        cmd_gnuplot_cr[0] = "set title 'Cores Ratio'";
        cmd_gnuplot_cr[1] = "set xrange [45:105]";
        cmd_gnuplot_cr[2] = "set yrange [1:1.06]";
        cmd_gnuplot_cr[3] = "set xlabel 'phi'";
        cmd_gnuplot_cr[4] = "set ylabel 'M/M*'";
        cmd_gnuplot_cr[5] = "set datafile separator whitespace";
        cmd_gnuplot_cr[6] = "plot 'data.bfdu_f_cr_phi' with linespoint lc 7 pointtype 7";
        plot_data(gnuplot_bfdu_f_cr, cmd_gnuplot_cr, 7);

        cmd_gnuplot_et[0] = "set title 'Execution Time (ms)'";
        cmd_gnuplot_et[1] = "set xrange [45:105]";
        cmd_gnuplot_et[2] = "set yrange [0:140]";
        cmd_gnuplot_et[3] = "set xlabel 'phi'";
        cmd_gnuplot_et[4] = "set ylabel 'milliseconds'";
        cmd_gnuplot_et[5] = "set datafile separator whitespace";
        cmd_gnuplot_et[6] = "plot 'data.bfdu_f_et_phi' with linespoint lc 7 pointtype 7";
        plot_data(gnuplot_bfdu_f_et, cmd_gnuplot_et, 7);

        cmd_gnuplot_sr[0] = "set title 'Schedulability Rate'";
        cmd_gnuplot_sr[1] = "set xrange [45:105]";
        cmd_gnuplot_sr[2] = "set yrange [0:100]";
        cmd_gnuplot_sr[3] = "set xlabel 'phi'";
        cmd_gnuplot_sr[4] = "set ylabel 'rate (%)'";
        cmd_gnuplot_sr[5] = "set datafile separator whitespace";
        cmd_gnuplot_sr[6] = "plot 'data.bfdu_f_sr_allo_phi' with linespoint lc 3 pointtype 7";
        cmd_gnuplot_sr[7] = "replot 'data.bfdu_f_sr_opti_phi' with linespoint lc 6 pointtype 7";
        plot_data(gnuplot_bfdu_f_sr, cmd_gnuplot_sr, 8);

        cmd_gnuplot_sr[0] = "set title 'Fragmentation Rate'";
        cmd_gnuplot_sr[1] = "set xrange [45:105]";
        cmd_gnuplot_sr[2] = "set yrange [0:40]";
        cmd_gnuplot_sr[3] = "set xlabel 'phi'";
        cmd_gnuplot_sr[4] = "set ylabel 'rate (%)'";
        cmd_gnuplot_sr[5] = "set datafile separator whitespace";
        cmd_gnuplot_sr[6] = "plot 'data.bfdu_f_fr_phi' with linespoint lc 7 pointtype 7";
        plot_data(gnuplot_bfdu_f_fr, cmd_gnuplot_fr, 7);

        cmd_gnuplot_op[0] = "set title 'Optimization Gain'";
        cmd_gnuplot_op[1] = "set style data histogram";
        cmd_gnuplot_op[2] = "set style fill solid";
        cmd_gnuplot_op[3] = "set style histogram clustered";
        cmd_gnuplot_op[4] = "set ylabel 'schedulabity gain (%)'";
        cmd_gnuplot_op[5] = "set yrange [0:20]";
        cmd_gnuplot_op[6] = "plot 'data.bfdu_f_op_phi' using 2:xtic(1) title 'reas gain',\
                             'data.bfdu_f_op_phi' using 3:xtic(1) title 'disp gain',\
                             'data.bfdu_f_op_phi' using 4:xtic(1) title 'swap gain',\
                             'data.bfdu_f_op_phi' using 5:xtic(1) title 'total gain'";
        plot_data(gnuplot_bfdu_f_op, cmd_gnuplot_op, 7);

        fflush(gnuplot_bfdu_f_cr);
        fflush(gnuplot_bfdu_f_fr);
        fflush(gnuplot_bfdu_f_et);
        fflush(gnuplot_bfdu_f_sr);
        fflush(gnuplot_bfdu_f_op);

        fclose(bfdu_f_cr);
        fclose(bfdu_f_fr);
        fclose(bfdu_f_et);
        fclose(bfdu_f_op);
        fclose(bfdu_f_sr_allo);
        fclose(bfdu_f_sr_opti);

        return 0;
}
