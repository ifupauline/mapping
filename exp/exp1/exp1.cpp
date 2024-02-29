#include "file.h"
#include "stats.h"
#include "generator.h"

#define STP 10
#define ITR 66

static const char *table = "data_table.txt";

static const char *table_hdr = "SIGMA, BFDU_M, SR_ALLO, SR_SWAP, SR_DISP,"
"WFDU_M, SR_ALLO, SR_SWAP, SR_DISP,"
"FFDU_M, SR_ALLO, SR_SWAP, SR_DISP";

static void _write_table(FILE *file, vector<struct results> &v_res, 
                const char *header)
{
        vector<float> v_float;

        write_header(file, header);

        for (unsigned int i = 0; i < v_res.size(); i++) {
                v_float.push_back(v_res[i].sigma);
                v_float.push_back(v_res[i].bfdu_m);
                v_float.push_back(v_res[i].bfdu_sr_allo);
                v_float.push_back(v_res[i].bfdu_sr_swap);
                v_float.push_back(v_res[i].bfdu_sr_disp);
                v_float.push_back(v_res[i].wfdu_m);
                v_float.push_back(v_res[i].wfdu_sr_allo);
                v_float.push_back(v_res[i].wfdu_sr_swap);
                v_float.push_back(v_res[i].wfdu_sr_disp);
                v_float.push_back(v_res[i].ffdu_m);
                v_float.push_back(v_res[i].ffdu_sr_allo);
                v_float.push_back(v_res[i].ffdu_sr_swap);
                v_float.push_back(v_res[i].ffdu_sr_disp);

                write_data(file, v_float);
                v_float.clear();
        }
}

static void exp1(vector<struct results> &v_res, struct params &prm)
{
        int sigma;
        struct context ctx;
        struct results res;
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

redo:   sigma = PHI;

        prm = {0};
        ctx = {0};
        res = {0};

        v_tcs.clear();

        gen_app(v_tcs, prm, ctx);

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

                ctx_bfdu.prm.s = sigma;
                ctx_wfdu.prm.s = sigma;
                ctx_ffdu.prm.s = sigma;

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

                assignment(v_tcs_bfdu, v_cores_bfdu, ctx_bfdu);
                if (STATE == FAILED) {
                        STATE = OK;
                        goto redo;
                }
                assignment(v_tcs_wfdu, v_cores_wfdu, ctx_wfdu);
                if (STATE == FAILED) {
                        STATE = OK;
                        goto redo;
                }
                assignment(v_tcs_ffdu, v_cores_ffdu, ctx_ffdu);
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

                stats(v_cores_bfdu, v_tcs_bfdu, ctx_bfdu);
                stats(v_cores_wfdu, v_tcs_wfdu, ctx_wfdu);
                stats(v_cores_ffdu, v_tcs_ffdu, ctx_ffdu);

                /* store results */
                res.bfdu_m = ctx_bfdu.cores_count;
                res.wfdu_m = ctx_wfdu.cores_count;
                res.ffdu_m = ctx_ffdu.cores_count;

                res.bfdu_sr_allo = ctx_bfdu.p.sched_rate_allo;
                res.wfdu_sr_allo = ctx_wfdu.p.sched_rate_allo;
                res.ffdu_sr_allo = ctx_ffdu.p.sched_rate_allo;

                res.bfdu_sr_disp = ctx_bfdu.p.sched_rate_disp * PERCENT;
                res.wfdu_sr_disp = ctx_wfdu.p.sched_rate_disp * PERCENT;
                res.ffdu_sr_disp = ctx_ffdu.p.sched_rate_disp * PERCENT;

                res.bfdu_sr_swap = ctx_bfdu.p.sched_rate_swap * PERCENT;
                res.wfdu_sr_swap = ctx_wfdu.p.sched_rate_swap * PERCENT;
                res.ffdu_sr_swap = ctx_ffdu.p.sched_rate_swap * PERCENT;

                res.sigma = (float)((float)sigma/PERMILL);

                v_res.push_back(res);

                sigma -= STP;
        }
}

int main(void)
{
        struct params prm;
        vector<struct results> v_res;

        exp1(v_res, prm);

        FILE *file1 = (FILE*)fopen(table, "w");

        _write_table(file1, v_res, table_hdr);

        fclose(file1);

        return 0;
}
