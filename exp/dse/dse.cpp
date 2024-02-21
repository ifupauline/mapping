#include "exp.h"
#include "print.h"
#include "generator.h"

#define STP 10
#define ITR 61

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

redo:   SIGMA = PHI;

        prm = {0};
        ctx = {0};
        stts = {0};

        v_tcs.clear();

        gen_app(v_tcs, prm, ctx);
        print_task_chains(v_tcs);

        for (int i = 0; i < ITR; i++) {
                ctx_bfdu = {0};
                ctx_wfdu = {0};
                ctx_ffdu = {0};

                ctx_bfdu = ctx;
                ctx_wfdu = ctx;
                ctx_ffdu = ctx;

                v_tcs_bfdu.clear();
                v_tcs_wfdu.clear();
                v_tcs_ffdu.clear();

                v_cores_bfdu.clear();
                v_cores_wfdu.clear();
                v_cores_ffdu.clear();

                v_tcs_wfdu = v_tcs;
                v_tcs_ffdu = v_tcs;
                v_tcs_bfdu = v_tcs;

                ctx_bfdu.prm.s = SIGMA;
                ctx_wfdu.prm.s = SIGMA;
                ctx_ffdu.prm.s = SIGMA;

                partitioning(v_tcs_bfdu, ctx_bfdu);
                partitioning(v_tcs_wfdu, ctx_wfdu);
                partitioning(v_tcs_ffdu, ctx_ffdu);

                init_ctx(v_tcs_bfdu, ctx_bfdu.prm, ctx_bfdu);
                init_ctx(v_tcs_wfdu, ctx_wfdu.prm, ctx_wfdu);
                init_ctx(v_tcs_ffdu, ctx_ffdu.prm, ctx_ffdu);

                ctx_bfdu.prm.a = BFDU;
                ctx_wfdu.prm.a = WFDU;
                ctx_ffdu.prm.a = FFDU;

                gen_arch(v_cores_bfdu, ctx_bfdu);
                gen_arch(v_cores_wfdu, ctx_wfdu);
                gen_arch(v_cores_ffdu, ctx_ffdu);

                allocation(v_tcs_bfdu, v_cores_bfdu, ctx_bfdu);
                if (STATE == FAILED) {
                        STATE = OK;
                        goto redo;
                }
                allocation(v_tcs_wfdu, v_cores_wfdu, ctx_wfdu);
                if (STATE == FAILED) {
                        STATE = OK;
                        goto redo;
                }
                allocation(v_tcs_ffdu, v_cores_ffdu, ctx_ffdu);
                if (STATE == FAILED) {
                        STATE = OK;
                        goto redo;
                }

                schedulability_analysis(v_cores_bfdu, ctx_bfdu);
                schedulability_analysis(v_cores_wfdu, ctx_wfdu);
                schedulability_analysis(v_cores_ffdu, ctx_ffdu);

                placement(v_cores_bfdu, ctx_bfdu);
                placement(v_cores_wfdu, ctx_wfdu);
                placement(v_cores_ffdu, ctx_ffdu);

                cmp_stats(v_cores_bfdu, v_tcs_bfdu, ctx_bfdu);
                cmp_stats(v_cores_wfdu, v_tcs_wfdu, ctx_wfdu);
                cmp_stats(v_cores_ffdu, v_tcs_ffdu, ctx_ffdu);

                stts.bfdu_m = ctx_bfdu.cores_count;
                stts.wfdu_m = ctx_wfdu.cores_count;
                stts.ffdu_m = ctx_ffdu.cores_count;

                stts.bfdu_sr_allo = ctx_bfdu.p.sched_rate_allo;
                stts.wfdu_sr_allo = ctx_wfdu.p.sched_rate_allo;
                stts.ffdu_sr_allo = ctx_ffdu.p.sched_rate_allo;

                stts.bfdu_sr_disp = ctx_bfdu.p.sched_rate_disp * PERCENT;
                stts.wfdu_sr_disp = ctx_wfdu.p.sched_rate_disp * PERCENT;
                stts.ffdu_sr_disp = ctx_ffdu.p.sched_rate_disp * PERCENT;

                stts.bfdu_sr_swap = ctx_bfdu.p.sched_rate_swap * PERCENT;
                stts.wfdu_sr_swap = ctx_wfdu.p.sched_rate_swap * PERCENT;
                stts.ffdu_sr_swap = ctx_ffdu.p.sched_rate_swap * PERCENT;

                stts.bfdu_et = ctx_bfdu.p.et * MSEC;
                stts.wfdu_et = ctx_wfdu.p.et * MSEC;
                stts.ffdu_et = ctx_ffdu.p.et * MSEC;

                stts.sig = (float)((float)SIGMA/PERMILL);

                v_stts.push_back(stts);

                printf("SIGMA: %d\n", SIGMA);
                printf("M:            %f\n", stts.bfdu_m);
                printf("M:            %f\n", stts.wfdu_m);
                printf("M:            %f\n", stts.ffdu_m);
                printf("SR_BFDU_ALLO: %f\n", stts.bfdu_sr_allo);
                printf("SR_WFDU_ALLO: %f\n", stts.wfdu_sr_allo);
                printf("SR_FFDU_ALLO: %f\n", stts.ffdu_sr_allo);
                printf("SR_SWAP:      %f\n", stts.bfdu_sr_swap);
                printf("SR_SWAP:      %f\n", stts.wfdu_sr_swap);
                printf("SR_SWAP:      %f\n", stts.ffdu_sr_swap);
                printf("SR_DISP:      %f\n", stts.bfdu_sr_disp);
                printf("SR_DISP:      %f\n", stts.wfdu_sr_disp);
                printf("SR_DISP:      %f\n", stts.ffdu_sr_disp);
                printf("ET_BFDU:      %f\n", stts.bfdu_et);
                printf("ET_WFDU:      %f\n", stts.bfdu_et);
                printf("ET_FFDU:      %f\n", stts.bfdu_et);

                SIGMA -= STP;
        }
}

int main(void)
{
        struct params prm;
        vector<struct b_stats> v_stts;

        dse(v_stts, prm);
        print_b_stats(v_stts, ITR);

        FILE *m = (FILE*)fopen("data.m_sig", "w");
        FILE *et = (FILE*)fopen("data.et_sig", "w");
        FILE *sr_allo = (FILE*)fopen("data.sr_allo_sig", "w");
        FILE *sr_disp = (FILE*)fopen("data.sr_disp_sig", "w");
        FILE *sr_swap = (FILE*)fopen("data.sr_swap_sig", "w");

        write_data_to_file(m, v_stts, B_M, ITR);
        write_data_to_file(et, v_stts, B_ET, ITR);
        write_data_to_file(sr_allo, v_stts, B_SR_ALLO, ITR);
        write_data_to_file(sr_disp, v_stts, B_SR_DISP, ITR);
        write_data_to_file(sr_swap, v_stts, B_SR_SWAP, ITR);

        fclose(m);
        fclose(et);
        fclose(sr_allo);
        fclose(sr_disp);
        fclose(sr_swap);

        return 0;
}
