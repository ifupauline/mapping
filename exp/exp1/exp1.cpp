#include "file.h"
#include "stats.h"
#include "generator.h"

#define STP 10
#define ITR 66
#define TCC 100

/*
 * test mapping methodology by conducting an exploration on parameters based on 
 * a synthetic use case 
 */
int EXP = EXP1;

static int sigma = 0;
static const char *synthetic  = "res_exp1.txt";
static const char *hdr_table = 
"SIGMA, FR,"
"BFDU_M, BFDU_SR_ALLO, BFDU_SR_SWAP, BFDU_SR_DISP, BFDU_PLAC_GAIN, BFDU_LETU, BFDU_ET_TOOL,"
"WFDU_M, WFDU_SR_ALLO, WFDU_SR_SWAP, WFDU_SR_DISP, WFDU_PLAC_GAIN, WFDU_LETU, WFDU_ET_TOOL,"
"FFDU_M, FFDU_SR_ALLO, FFDU_SR_SWAP, FFDU_SR_DISP, FFDU_PLAC_GAIN, FFDU_LETU, FFDU_ET_TOOL,"
"BFDU_IC, BFDU_IE, BFDU_SR_TC, BFDU_E2E_AVG, BFDU_E2E_MAX, BFDU_ET_COMM,"
"WFDU_IC, WFDU_IE, WFDU_SR_TC, WFDU_E2E_AVG, WFDU_E2E_MAX, WFDU_ET_COMM,"
"FFDU_IC, FFDU_IE, FFDU_SR_TC, FFDU_E2E_AVG, FFDU_E2E_MAX, FFDU_ET_COMM";

/* communication metrics of one mapping (appended after the original columns) */
static void _store_comm(float &ic, float &ie, float &sr_tc, float &e2e_avg,
                float &e2e_max, float &et_comm, struct context &ctx)
{
        ic = ctx.intra_comm_count;
        ie = ctx.inter_comm_count;
        sr_tc = ctx.p.sched_rate_tc;
        e2e_avg = ctx.p.e2e_avg;
        e2e_max = ctx.p.e2e_max;
        et_comm = ctx.p.comm_time * MSEC;
}

static void _store_results(vector<struct results> &v_res, struct results &res, 
                struct context &ctx_bfdu, struct context &ctx_wfdu, 
                struct context &ctx_ffdu)
{
        res.bfdu_fr = ctx_bfdu.p.fr;
        res.wfdu_fr = ctx_wfdu.p.fr;
        res.ffdu_fr = ctx_ffdu.p.fr;

        res.bfdu_letu = (ctx_bfdu.p.letu / ctx_bfdu.p.sysu) * PERCENT;
        res.wfdu_letu = (ctx_wfdu.p.letu / ctx_wfdu.p.sysu) * PERCENT;
        res.ffdu_letu = (ctx_ffdu.p.letu / ctx_ffdu.p.sysu) * PERCENT;

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

        res.bfdu_plac_gain = ctx_bfdu.p.plac_gain;
        res.wfdu_plac_gain = ctx_wfdu.p.plac_gain;
        res.ffdu_plac_gain = ctx_ffdu.p.plac_gain;

        res.bfdu_et_tool = ctx_bfdu.p.exec_time * MSEC;
        res.wfdu_et_tool = ctx_wfdu.p.exec_time * MSEC;
        res.ffdu_et_tool = ctx_ffdu.p.exec_time * MSEC;

        _store_comm(res.bfdu_ic, res.bfdu_ie, res.bfdu_sr_tc, res.bfdu_e2e_avg,
                        res.bfdu_e2e_max, res.bfdu_et_comm, ctx_bfdu);
        _store_comm(res.wfdu_ic, res.wfdu_ie, res.wfdu_sr_tc, res.wfdu_e2e_avg,
                        res.wfdu_e2e_max, res.wfdu_et_comm, ctx_wfdu);
        _store_comm(res.ffdu_ic, res.ffdu_ie, res.ffdu_sr_tc, res.ffdu_e2e_avg,
                        res.ffdu_e2e_max, res.ffdu_et_comm, ctx_ffdu);

        res.sigma = (float)((float)sigma/PERMILL);
        v_res.push_back(res);
}

static void _write_results(vector<struct results> &v_res, const char *filename)
{
        FILE *file;
        vector<float> v_float;

        printf("\nWriting Results to %s\n", filename);

        file = (FILE*)fopen(filename, "w");

        write_header(file, hdr_table);

        for (unsigned int i = 0; i < v_res.size(); i++) {
                v_float.push_back(v_res[i].sigma);
                v_float.push_back(v_res[i].bfdu_fr);
                v_float.push_back(v_res[i].bfdu_m);
                v_float.push_back(v_res[i].bfdu_sr_allo);
                v_float.push_back(v_res[i].bfdu_sr_swap);
                v_float.push_back(v_res[i].bfdu_sr_disp);
                v_float.push_back(v_res[i].bfdu_plac_gain);
                v_float.push_back(v_res[i].bfdu_letu);
                v_float.push_back(v_res[i].bfdu_et_tool);
                v_float.push_back(v_res[i].wfdu_m);
                v_float.push_back(v_res[i].wfdu_sr_allo);
                v_float.push_back(v_res[i].wfdu_sr_swap);
                v_float.push_back(v_res[i].wfdu_sr_disp);
                v_float.push_back(v_res[i].wfdu_plac_gain);
                v_float.push_back(v_res[i].wfdu_letu);
                v_float.push_back(v_res[i].wfdu_et_tool);
                v_float.push_back(v_res[i].ffdu_m);
                v_float.push_back(v_res[i].ffdu_sr_allo);
                v_float.push_back(v_res[i].ffdu_sr_swap);
                v_float.push_back(v_res[i].ffdu_sr_disp);
                v_float.push_back(v_res[i].ffdu_plac_gain);
                v_float.push_back(v_res[i].ffdu_letu);
                v_float.push_back(v_res[i].ffdu_et_tool);

                v_float.push_back(v_res[i].bfdu_ic);
                v_float.push_back(v_res[i].bfdu_ie);
                v_float.push_back(v_res[i].bfdu_sr_tc);
                v_float.push_back(v_res[i].bfdu_e2e_avg);
                v_float.push_back(v_res[i].bfdu_e2e_max);
                v_float.push_back(v_res[i].bfdu_et_comm);
                v_float.push_back(v_res[i].wfdu_ic);
                v_float.push_back(v_res[i].wfdu_ie);
                v_float.push_back(v_res[i].wfdu_sr_tc);
                v_float.push_back(v_res[i].wfdu_e2e_avg);
                v_float.push_back(v_res[i].wfdu_e2e_max);
                v_float.push_back(v_res[i].wfdu_et_comm);
                v_float.push_back(v_res[i].ffdu_ic);
                v_float.push_back(v_res[i].ffdu_ie);
                v_float.push_back(v_res[i].ffdu_sr_tc);
                v_float.push_back(v_res[i].ffdu_e2e_avg);
                v_float.push_back(v_res[i].ffdu_e2e_max);
                v_float.push_back(v_res[i].ffdu_et_comm);

                write_data(file, v_float);
                v_float.clear();
        }
        fclose(file);
}

static void _synthetic(vector<struct results> &v_res)
{
        struct context ctx;
        struct results res;
        struct params prm;
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
        vector<struct tc_comm> v_comm_bfdu;
        vector<struct tc_comm> v_comm_wfdu;
        vector<struct tc_comm> v_comm_ffdu;

redo:   sigma = PHI;

        /* drop results of a previously generated (unfeasible) application */
        v_res.clear();

        ctx = {0};
        res = {0};
        prm = {0};

        v_tcs.clear();

        gen_app(v_tcs, prm, ctx);

        for (unsigned int i = 0; i < ITR; i++) {
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

                communication(v_cores_bfdu, v_comm_bfdu, ctx_bfdu);
                communication(v_cores_wfdu, v_comm_wfdu, ctx_wfdu);
                communication(v_cores_ffdu, v_comm_ffdu, ctx_ffdu);

                stats(v_cores_bfdu, v_tcs_bfdu, ctx_bfdu);
                stats(v_cores_wfdu, v_tcs_wfdu, ctx_wfdu);
                stats(v_cores_ffdu, v_tcs_ffdu, ctx_ffdu);

                _store_results(v_res, res, ctx_bfdu, ctx_wfdu, ctx_ffdu);

                sigma -= STP;
        }
        _write_results(v_res, synthetic);
}

int main(void)
{
        vector<struct results> v_res_s;

        _synthetic(v_res_s);

        return 0;
}
