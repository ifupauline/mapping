#include "exp.h"
#include "print.h"
#include "generator.h"

#define STEP    20
#define ITERSIG 16
#define SIMNBR  1000

static char const *cmd_gnuplot_m[] = {};

static void dse(vector<struct b_stats> &v_stts_bfdu, struct params &prm)
{
        int SIGMA;
        struct context ctx;
        struct context ctx_bfdu;
        struct b_stats stts_bfdu;
        vector<struct bin> v_bins;
        vector<struct bin> v_bins_bfdu;
        vector<struct item> v_itms;
        vector<struct item> v_itms_bfdu;
        vector<struct ecu> v_ecus_bfdu;

        prm = {0};
        SIGMA = 820;
        for (int j = 0; j < ITERSIG; j++) {
                v_itms.clear();
                ctx = {0};
                stts_bfdu = {0};
                SIGMA = SIGMA - STEP;
                gen_case_study(v_itms, prm, ctx);
                for (int k = 0; k < SIMNBR; k++) {
                        ctx_bfdu = {0};
                        v_itms_bfdu.clear();
                        v_bins_bfdu.clear();
                        v_itms_bfdu = v_itms;
                        v_ecus_bfdu.clear();
                        ctx_bfdu = ctx;
                        ctx_bfdu.prm.e = SIGMA;

                        fragmentation(v_itms_bfdu, ctx_bfdu);
                        init_ctx(v_itms_bfdu, ctx_bfdu.prm, ctx_bfdu);
                        ctx_bfdu.prm.a = BFDU_F;
                        generation(v_bins_bfdu, ctx_bfdu);
                        allocation(v_itms_bfdu, v_bins_bfdu, ctx_bfdu);
                        schedulability_analysis(v_bins_bfdu, ctx_bfdu);
                        optimization(v_bins_bfdu, ctx_bfdu);
                        communication(v_bins_bfdu, v_itms_bfdu, v_ecus_bfdu, ctx_bfdu);
                        cmp_stats(v_bins_bfdu, v_itms_bfdu, ctx_bfdu);
                        stts_bfdu.mean_m += ctx_bfdu.bins_count;
                }
                /* mean */
                stts_bfdu.mean_m /= (float)SIMNBR;
                stts_bfdu.sig = SIGMA;
                v_stts_bfdu.push_back(stts_bfdu);
                printf("SIGMA: %d\n", SIGMA);
                printf("M: %f\n", stts_bfdu.mean_m);
        }
}

int main(void)
{
        struct params prm;
        vector<struct b_stats> v_stts_bfdu;

        dse(v_stts_bfdu, prm);
        print_b_stats(v_stts_bfdu, ITERSIG);

        FILE *gnuplot_bfdu_m = (FILE*)popen("gnuplot -persistent", "w");
        FILE *bfdu_m = (FILE*)fopen("data.bfdu_m_sig", "w");

        write_data_to_file(bfdu_m, v_stts_bfdu, B_M, ITERSIG);

        cmd_gnuplot_m[0] = "set title 'M'";
        cmd_gnuplot_m[1] = "set xrange [480:820]";
        cmd_gnuplot_m[2] = "set yrange [0:30]";
        cmd_gnuplot_m[3] = "set xlabel 'sigma'";
        cmd_gnuplot_m[4] = "set ylabel 'M'";
        cmd_gnuplot_m[5] = "set datafile separator whitespace";
        cmd_gnuplot_m[6] = "plot 'data.bfdu_m_sig' with linespoint lc 7 pointtype 7";
        plot_data(gnuplot_bfdu_m, cmd_gnuplot_m, 7);

        fflush(gnuplot_bfdu_m);

        fclose(bfdu_m);

        return 0;
}
