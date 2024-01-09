#include "exp.h"
#include "print.h"
#include "generator.h"

#define STEP    20
#define ITERSIG 16
#define SIMNBR  1000

static char const *cmd_gnuplot_m[] = {};
static char const *cmd_gnuplot_sr[] = {};
static char const *cmd_gnuplot_et[] = {};

static void dse(vector<struct b_stats> &v_stts, struct params &prm)
{
        int SIGMA;
        struct context ctx;
        struct b_stats stts;
        vector<struct core> v_cores;
        vector<struct tc> v_tcs;
        struct context ctx_bfdu;
        struct context ctx_wfdu;
        struct context ctx_ffdu;
        vector<struct core> v_cores_bfdu;
        vector<struct core> v_cores_wfdu;
        vector<struct core> v_cores_ffdu;
        vector<struct tc> v_tcs_bfdu;
        vector<struct tc> v_tcs_wfdu;
        vector<struct tc> v_tcs_ffdu;

        prm = {0};
        SIGMA = 820;
        for (int j = 0; j < ITERSIG; j++) {
                v_tcs.clear();
                ctx = {0};
                stts = {0};
                SIGMA = SIGMA - STEP;
                gen_app(v_tcs, prm, ctx);
                for (int k = 0; k < SIMNBR; k++) {
                        ctx_bfdu = {0};
                        v_tcs_bfdu.clear();
                        v_cores_bfdu.clear();
                        v_tcs_bfdu = v_tcs;
                        ctx_bfdu = ctx;
                        ctx_bfdu.prm.s = SIGMA;

                        v_tcs_wfdu.clear();
                        v_cores_wfdu.clear();
                        v_tcs_wfdu = v_tcs;
                        ctx_wfdu = ctx;
                        ctx_wfdu.prm.s = SIGMA;

                        v_tcs_ffdu.clear();
                        v_cores_ffdu.clear();
                        v_tcs_ffdu = v_tcs;
                        ctx_ffdu = ctx;
                        ctx_ffdu.prm.s = SIGMA;

                        partitioning(v_tcs_bfdu, ctx_bfdu);
                        partitioning(v_tcs_wfdu, ctx_wfdu);
                        partitioning(v_tcs_ffdu, ctx_ffdu);

                        init_ctx(v_tcs_bfdu, ctx_bfdu.prm, ctx_bfdu);
                        init_ctx(v_tcs_wfdu, ctx_wfdu.prm, ctx_wfdu);
                        init_ctx(v_tcs_ffdu, ctx_ffdu.prm, ctx_ffdu);

                        ctx_bfdu.prm.a = BFDU_F;
                        ctx_wfdu.prm.a = WFDU_F;
                        ctx_ffdu.prm.a = FFDU_F;

                        gen_arch(v_cores_bfdu, ctx_bfdu);
                        gen_arch(v_cores_wfdu, ctx_wfdu);
                        gen_arch(v_cores_ffdu, ctx_ffdu);

                        allocation(v_tcs_bfdu, v_cores_bfdu, ctx_bfdu);
                        allocation(v_tcs_wfdu, v_cores_wfdu, ctx_wfdu);
                        allocation(v_tcs_ffdu, v_cores_ffdu, ctx_ffdu);

                        schedulability_analysis(v_cores_bfdu, ctx_bfdu);
                        schedulability_analysis(v_cores_wfdu, ctx_wfdu);
                        schedulability_analysis(v_cores_ffdu, ctx_ffdu);

                        placement(v_cores_bfdu, ctx_bfdu);
                        placement(v_cores_wfdu, ctx_wfdu);
                        placement(v_cores_ffdu, ctx_ffdu);

                        cmp_stats(v_cores_bfdu, v_tcs_bfdu, ctx_bfdu);
                        cmp_stats(v_cores_wfdu, v_tcs_wfdu, ctx_wfdu);
                        cmp_stats(v_cores_ffdu, v_tcs_ffdu, ctx_ffdu);

                        stts.mean_bfdu_m += ctx_bfdu.cores_count;
                        stts.mean_wfdu_m += ctx_wfdu.cores_count;
                        stts.mean_ffdu_m += ctx_ffdu.cores_count;

                        stts.mean_bfdu_sr_allo += ctx_bfdu.p.sched_rate_allo;
                        stts.mean_wfdu_sr_allo += ctx_wfdu.p.sched_rate_allo;
                        stts.mean_ffdu_sr_allo += ctx_ffdu.p.sched_rate_allo;

                        stts.mean_bfdu_sr_disp += ctx_bfdu.p.sched_rate_disp * PERCENT;
                        stts.mean_wfdu_sr_disp += ctx_wfdu.p.sched_rate_disp * PERCENT;
                        stts.mean_ffdu_sr_disp += ctx_ffdu.p.sched_rate_disp * PERCENT;

                        stts.mean_bfdu_sr_swap += ctx_bfdu.p.sched_rate_swap * PERCENT;
                        stts.mean_wfdu_sr_swap += ctx_wfdu.p.sched_rate_swap * PERCENT;
                        stts.mean_ffdu_sr_swap += ctx_ffdu.p.sched_rate_swap * PERCENT;

                        stts.mean_bfdu_et += ctx_bfdu.p.et * MSEC;
                        stts.mean_wfdu_et += ctx_wfdu.p.et * MSEC;
                        stts.mean_ffdu_et += ctx_ffdu.p.et * MSEC;
                }
                /* mean */
                stts.mean_bfdu_m /= (float)SIMNBR;
                stts.mean_wfdu_m /= (float)SIMNBR;
                stts.mean_ffdu_m /= (float)SIMNBR;

                stts.mean_bfdu_sr_allo /= (float)SIMNBR;
                stts.mean_wfdu_sr_allo /= (float)SIMNBR;
                stts.mean_ffdu_sr_allo /= (float)SIMNBR;

                stts.mean_bfdu_sr_disp /= (float)SIMNBR;
                stts.mean_wfdu_sr_disp /= (float)SIMNBR;
                stts.mean_ffdu_sr_disp /= (float)SIMNBR;

                stts.mean_bfdu_sr_swap /= (float)SIMNBR;
                stts.mean_wfdu_sr_swap /= (float)SIMNBR;
                stts.mean_ffdu_sr_swap /= (float)SIMNBR;

                stts.mean_bfdu_et /= (float)SIMNBR;
                stts.mean_wfdu_et /= (float)SIMNBR;
                stts.mean_ffdu_et /= (float)SIMNBR;

                stts.mean_et = stts.mean_bfdu_et + stts.mean_wfdu_et + stts.mean_ffdu_et;
                stts.mean_et /= 3;

                stts.sig = (float)((float)SIGMA/PERMILL);
                v_stts.push_back(stts);
                printf("SIGMA: %d\n", SIGMA);
                printf("M:            %f\n", stts.mean_bfdu_m);
                printf("M:            %f\n", stts.mean_wfdu_m);
                printf("M:            %f\n", stts.mean_ffdu_m);
                printf("SR_BFDU_ALLO: %f\n", stts.mean_bfdu_sr_allo);
                printf("SR_WFDU_ALLO: %f\n", stts.mean_wfdu_sr_allo);
                printf("SR_FFDU_ALLO: %f\n", stts.mean_ffdu_sr_allo);
                printf("SR_DISP:      %f\n", stts.mean_bfdu_sr_disp);
                printf("SR_DISP:      %f\n", stts.mean_wfdu_sr_disp);
                printf("SR_DISP:      %f\n", stts.mean_ffdu_sr_disp);
                printf("SR_SWAP:      %f\n", stts.mean_bfdu_sr_swap);
                printf("SR_SWAP:      %f\n", stts.mean_wfdu_sr_swap);
                printf("SR_SWAP:      %f\n", stts.mean_ffdu_sr_swap);
                printf("ET_BFDU:      %f\n", stts.mean_bfdu_et);
                printf("ET_WFDU:      %f\n", stts.mean_bfdu_et);
                printf("ET_FFDU:      %f\n", stts.mean_bfdu_et);
        }
}

int main(void)
{
        struct params prm;
        vector<struct b_stats> v_stts;

        dse(v_stts, prm);
        print_b_stats(v_stts, ITERSIG);

        FILE *gnuplot_m = (FILE*)popen("gnuplot -persistent", "w");
        FILE *gnuplot_sr = (FILE*)popen("gnuplot -persistent", "w");
        FILE *gnuplot_et = (FILE*)popen("gnuplot -persistent", "w");
        FILE *m = (FILE*)fopen("data.m_sig", "w");
        FILE *sr_allo = (FILE*)fopen("data.sr_allo_sig", "w");
        FILE *sr_disp = (FILE*)fopen("data.sr_disp_sig", "w");
        FILE *sr_swap = (FILE*)fopen("data.sr_swap_sig", "w");
        FILE *et = (FILE*)fopen("data.et_sig", "w");

        write_data_to_file(m, v_stts, B_M, ITERSIG);
        write_data_to_file(sr_allo, v_stts, B_SR_ALLO, ITERSIG);
        write_data_to_file(sr_disp, v_stts, B_SR_DISP, ITERSIG);
        write_data_to_file(sr_swap, v_stts, B_SR_SWAP, ITERSIG);
        write_data_to_file(et, v_stts, B_ET, ITERSIG);

        cmd_gnuplot_m[0] = "set title 'Number of cores in use'";
        cmd_gnuplot_m[1] = "set style data histogram";
        cmd_gnuplot_m[2] = "set style fill solid";
        cmd_gnuplot_m[3] = "set style histogram clustered";
        cmd_gnuplot_m[4] = "set yrange [0:50]";
        cmd_gnuplot_m[5] = "set xlabel 'sigma'";
        cmd_gnuplot_m[6] = "set ylabel 'M'";
        cmd_gnuplot_m[7] = "set auto x";
        cmd_gnuplot_m[8] = "plot 'data.m_sig' using 2:xtic(1) title 'bfdu',\
                            'data.m_sig' using 3:xtic(1) title 'wfdu',\
                            'data.m_sig' using 4:xtic(1) title 'ffdu'";

        plot_data(gnuplot_m, cmd_gnuplot_m, 9);

        cmd_gnuplot_sr[0] = "set title 'Schedulability Rate'";
        cmd_gnuplot_sr[1] = "set yrange [0:130]";
        cmd_gnuplot_sr[2] = "set xlabel 'sigma'";
        cmd_gnuplot_sr[3] = "set ylabel 'SR (%)'";
        cmd_gnuplot_sr[4] = "set datafile separator whitespace";
        cmd_gnuplot_sr[5] = "set auto x";
        cmd_gnuplot_sr[6] = "plot 'data.sr_allo_sig' u 1:2 title 'allo' with linespoint lc 7 linewidth 2 pointtype 7 ps 1";
        cmd_gnuplot_sr[7] = "replot 'data.sr_swap_sig' u 1:2 title 'opti' with linespoint lc 2 linewidth 2 pointtype 7 ps 1";

        plot_data(gnuplot_sr, cmd_gnuplot_sr, 8);

        cmd_gnuplot_m[0] = "set title 'Execution Time (tool)'";
        cmd_gnuplot_m[1] = "set style data histogram";
        cmd_gnuplot_m[2] = "set style fill solid";
        cmd_gnuplot_m[3] = "set style histogram clustered";
        cmd_gnuplot_m[4] = "set yrange [0:50]";
        cmd_gnuplot_m[5] = "set xlabel 'sigma'";
        cmd_gnuplot_m[6] = "set ylabel 'miliseconds'";
        cmd_gnuplot_m[7] = "set auto x";
        cmd_gnuplot_m[8] = "plot 'data.et_sig' using 2:xtic(1) title 'average' linecolor 6";

        plot_data(gnuplot_et, cmd_gnuplot_et, 9);

        fflush(gnuplot_m);
        fflush(gnuplot_sr);
        fflush(gnuplot_et);

        fclose(m);
        fclose(et);
        fclose(sr_allo);
        fclose(sr_disp);
        fclose(sr_swap);

        return 0;
}
