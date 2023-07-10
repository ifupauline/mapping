#include "exp.h"
#include "print.h"
#include "generator.h"

#define STEP    20
#define ITERSIG 16
#define SIMNBR  1

static char const *cmd_gnuplot_m[] = {};
//static char const *cmd_gnuplot_sr[] = {};

static void dse(vector<struct b_stats> &v_stts, struct params &prm)
{
        int SIGMA;
        struct context ctx;
        struct context ctx_bfdu;
        struct b_stats stts;
        vector<struct bin> v_bins;
        vector<struct bin> v_bins_bfdu;
        vector<struct item> v_itms;
        vector<struct item> v_itms_bfdu;
        vector<struct ecu> v_ecus_bfdu;

        struct context ctx_wfdu;
        vector<struct bin> v_bins_wfdu;
        vector<struct item> v_itms_wfdu;
        vector<struct ecu> v_ecus_wfdu;

        struct context ctx_ffdu;
        vector<struct bin> v_bins_ffdu;
        vector<struct item> v_itms_ffdu;
        vector<struct ecu> v_ecus_ffdu;

        prm = {0};
        SIGMA = 820;
        for (int j = 0; j < ITERSIG; j++) {
                v_itms.clear();
                ctx = {0};
                stts = {0};
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

                        v_itms_wfdu.clear();
                        v_bins_wfdu.clear();
                        v_itms_wfdu = v_itms;
                        v_ecus_wfdu.clear();
                        ctx_wfdu = ctx;
                        ctx_wfdu.prm.e = SIGMA;

                        v_itms_ffdu.clear();
                        v_bins_ffdu.clear();
                        v_itms_ffdu = v_itms;
                        v_ecus_ffdu.clear();
                        ctx_ffdu = ctx;
                        ctx_ffdu.prm.e = SIGMA;

                        fragmentation(v_itms_bfdu, ctx_bfdu);
                        fragmentation(v_itms_wfdu, ctx_wfdu);
                        fragmentation(v_itms_ffdu, ctx_ffdu);

                        init_ctx(v_itms_bfdu, ctx_bfdu.prm, ctx_bfdu);
                        init_ctx(v_itms_wfdu, ctx_wfdu.prm, ctx_wfdu);
                        init_ctx(v_itms_ffdu, ctx_ffdu.prm, ctx_ffdu);

                        ctx_bfdu.prm.a = BFDU_F;
                        ctx_wfdu.prm.a = WFDU_F;
                        ctx_ffdu.prm.a = FFDU_F;

                        generation(v_bins_bfdu, ctx_bfdu);
                        generation(v_bins_wfdu, ctx_wfdu);
                        generation(v_bins_ffdu, ctx_ffdu);

                        allocation(v_itms_bfdu, v_bins_bfdu, ctx_bfdu);
                        allocation(v_itms_wfdu, v_bins_wfdu, ctx_wfdu);
                        allocation(v_itms_ffdu, v_bins_ffdu, ctx_ffdu);

                        schedulability_analysis(v_bins_bfdu, ctx_bfdu);
                        schedulability_analysis(v_bins_wfdu, ctx_wfdu);
                        schedulability_analysis(v_bins_ffdu, ctx_ffdu);

                        optimization(v_bins_bfdu, ctx_bfdu);
                        optimization(v_bins_wfdu, ctx_wfdu);
                        optimization(v_bins_ffdu, ctx_ffdu);

                        communication(v_bins_bfdu, v_itms_bfdu, v_ecus_bfdu, ctx_bfdu);
                        communication(v_bins_wfdu, v_itms_wfdu, v_ecus_wfdu, ctx_wfdu);
                        communication(v_bins_ffdu, v_itms_ffdu, v_ecus_ffdu, ctx_ffdu);

                        cmp_stats(v_bins_bfdu, v_itms_bfdu, ctx_bfdu);
                        cmp_stats(v_bins_wfdu, v_itms_wfdu, ctx_wfdu);
                        cmp_stats(v_bins_ffdu, v_itms_ffdu, ctx_ffdu);

                        stts.mean_bfdu_m += ctx_bfdu.bins_count;
                        stts.mean_wfdu_m += ctx_wfdu.bins_count;
                        stts.mean_ffdu_m += ctx_ffdu.bins_count;
                        
                        stts.mean_bfdu_sr_allo += ctx_bfdu.p.sched_rate_allo;
                        stts.mean_wfdu_sr_allo += ctx_wfdu.p.sched_rate_allo;
                        stts.mean_ffdu_sr_allo += ctx_ffdu.p.sched_rate_allo;

                        stts.mean_bfdu_sr_disp += ctx_bfdu.p.sched_rate_disp * PERCENT;
                        stts.mean_wfdu_sr_disp += ctx_wfdu.p.sched_rate_disp * PERCENT;
                        stts.mean_ffdu_sr_disp += ctx_ffdu.p.sched_rate_disp * PERCENT;

                        stts.mean_bfdu_sr_swap += ctx_bfdu.p.sched_rate_swap * PERCENT;
                        stts.mean_wfdu_sr_swap += ctx_wfdu.p.sched_rate_swap * PERCENT;
                        stts.mean_ffdu_sr_swap += ctx_ffdu.p.sched_rate_swap * PERCENT;
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

                stts.sig = SIGMA;
                v_stts.push_back(stts);
                printf("SIGMA: %d\n", SIGMA);
                printf("M: %f\n", stts.mean_bfdu_m);
                printf("M: %f\n", stts.mean_wfdu_m);
                printf("M: %f\n", stts.mean_ffdu_m);
                printf("SR_ALLO: %f\n", stts.mean_bfdu_sr_allo);
                printf("SR_ALLO: %f\n", stts.mean_wfdu_sr_allo);
                printf("SR_ALLO: %f\n", stts.mean_ffdu_sr_allo);
                printf("SR_DISP: %f\n", stts.mean_bfdu_sr_disp);
                printf("SR_DISP: %f\n", stts.mean_wfdu_sr_disp);
                printf("SR_DISP: %f\n", stts.mean_ffdu_sr_disp);
                printf("SR_SWAP: %f\n", stts.mean_bfdu_sr_swap);
                printf("SR_SWAP: %f\n", stts.mean_wfdu_sr_swap);
                printf("SR_SWAP: %f\n", stts.mean_ffdu_sr_swap);
        }
}

int main(void)
{
        struct params prm;
        vector<struct b_stats> v_stts;

        dse(v_stts, prm);
        print_b_stats(v_stts, ITERSIG);

        FILE *gnuplot_m = (FILE*)popen("gnuplot -persistent", "w");
        FILE *gnuplot_sr_allo = (FILE*)popen("gnuplot -persistent", "w");
        FILE *gnuplot_sr_disp = (FILE*)popen("gnuplot -persistent", "w");
        FILE *gnuplot_sr_swap = (FILE*)popen("gnuplot -persistent", "w");
        FILE *m = (FILE*)fopen("data.m_sig", "w");
        FILE *sr_allo = (FILE*)fopen("data.sr_allo_sig", "w");
        FILE *sr_disp = (FILE*)fopen("data.sr_disp_sig", "w");
        FILE *sr_swap = (FILE*)fopen("data.sr_swap_sig", "w");

        write_data_to_file(m, v_stts, B_M, ITERSIG);
        write_data_to_file(sr_allo, v_stts, B_SR_ALLO, ITERSIG);
        write_data_to_file(sr_disp, v_stts, B_SR_DISP, ITERSIG);
        write_data_to_file(sr_swap, v_stts, B_SR_SWAP, ITERSIG);

        cmd_gnuplot_m[0] = "set title 'M'";
        cmd_gnuplot_m[1] = "set auto x";
        cmd_gnuplot_m[2] = "set yrange [0:30]";
        cmd_gnuplot_m[3] = "set xrange [820:480]";
        cmd_gnuplot_m[4] = "set style data histogram";
        cmd_gnuplot_m[5] = "set style histogram cluster gap 1";
        cmd_gnuplot_m[6] = "set style fill solid border -1";
        cmd_gnuplot_m[7] = "set boxwidth 0.9";
        cmd_gnuplot_m[8] = "set xtic rotate by -45 scale 0";
        cmd_gnuplot_m[9] = "plot 'data.m_sig' using 6:xtic(1) ti col, '' u 12 ti col, '' u 13 ti col, '' u 14 ti col";

        plot_data(gnuplot_m, cmd_gnuplot_m, 10);

        fflush(gnuplot_m);
        fflush(gnuplot_sr_allo);
        fflush(gnuplot_sr_disp);
        fflush(gnuplot_sr_swap);

        fclose(m);
        fclose(sr_allo);
        fclose(sr_disp);
        fclose(sr_swap);

        return 0;
}
