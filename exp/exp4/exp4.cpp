#include "file.h"
#include "stats.h"
#include "generator.h"

#define TCC 5000

int EXP = EXP4;

static int sigma = 0;

static const char *synthetic  = "res_exp4.txt";

static const char *hdr_table =
"SIGMA, FR, TASK_COUNT, BFDU_ET_TOOL";

static void _store_results(vector<struct results> &v_res, struct results &res, 
                struct context &ctx_bfdu, struct context &ctx_wfdu, 
                struct context &ctx_ffdu)
{
        res.sigma = (float)((float)sigma/PERMILL);
        res.tasks_count = ctx_bfdu.tasks_count;
        res.bfdu_fr = ctx_bfdu.p.fr;
        res.bfdu_et_tool = ctx_bfdu.p.exec_time * MSEC;
        v_res.push_back(res);
}

static void _write_results(vector<struct results> &v_res)
{
        FILE *file;
        vector<float> v_float;

        printf("\nWriting Results to %s\n", synthetic);

        file = (FILE*)fopen(synthetic, "w");

        write_header(file, hdr_table);

        for (unsigned int i = 0; i < v_res.size(); i++) {
                v_float.push_back(v_res[i].sigma);
                v_float.push_back(v_res[i].bfdu_fr);
                v_float.push_back(v_res[i].tasks_count);
                v_float.push_back(v_res[i].bfdu_et_tool);
                write_data(file, v_float);
                v_float.clear();
        }
        fclose(file);
}

static void _gen_app(vector<struct tc> &v_tcs, struct context &ctx)
{
        struct tc tc_white;

        for (int i = 0; i < ctx.prm.m; i++) {
                tc_white = {0};
                gen_tc(tc_white, WHITE, 1, 1000);
                v_tcs.push_back(tc_white);
        }
        sort_dec_tc_size(v_tcs);
        assign_ids(v_tcs);

        for (unsigned int i = 0; i < v_tcs.size(); i++)
                v_tcs[i].gcd = gcd(v_tcs[i].v_tasks);
}

static void _gen_arch(vector<struct core> &v_cores, struct context &ctx)
{
        for (int i = 0; i < ctx.prm.m; i++)
                add_core(v_cores, RED, 1, ctx);
}

static void _synthetic(vector<struct results> &v_res)
{
        struct context ctx;
        struct results res;
        vector<struct core> v_cores;
        vector<struct tc> v_tcs;
        struct context ctx_bfdu;
        struct context ctx_wfdu;
        struct context ctx_ffdu;
        vector<struct core> v_cores_bfdu;
        vector<struct tc> v_tcs_bfdu;

redo:   sigma = PHI / 2;

        ctx = {0};
        res = {0};

        v_tcs.clear();

        ctx.prm.m = TCC;
        _gen_app(v_tcs, ctx);

        ctx_bfdu = {0};
        ctx_bfdu = ctx;

        v_tcs_bfdu.clear();
        v_tcs_bfdu = v_tcs;

        v_cores_bfdu.clear();

        ctx_bfdu.prm.a = BFDU;
        ctx_bfdu.prm.s = sigma;

        partitioning(v_tcs_bfdu, ctx_bfdu);
        init_ctx(v_tcs_bfdu, ctx_bfdu.prm, ctx_bfdu);
        _gen_arch(v_cores_bfdu, ctx_bfdu);

        assignment(v_tcs_bfdu, v_cores_bfdu, ctx_bfdu);
        if (STATE == FAILED) {
                STATE = OK;
                goto redo;
        }

        schedulability_analysis(v_cores_bfdu, ctx_bfdu);
        placement(v_cores_bfdu, ctx_bfdu);
        stats(v_cores_bfdu, v_tcs_bfdu, ctx_bfdu);
        _store_results(v_res, res, ctx_bfdu, ctx_wfdu, ctx_ffdu);
        _write_results(v_res);
}

int main(void)
{
        vector<struct results> v_res_s;

        _synthetic(v_res_s);

        return 0;
}
