#include "file.h"
#include "stats.h"
#include "print.h"
#include "generator.h"

#define ITR   10
#define STP   30
#define SIGMA 400

static const char *table = "res_exp2.txt";

static const char *hdr_table = 
"m,"
"ET_PART (ms),"
"ET_ASSI (ms),"
"ET_SCHD (ms),"
"ET_SWAP (ms),"
"ET_DISP (ms),"
"ET_TOOL (ms)";

static void _gen_app(vector<struct tc> &v_tcs, struct context &ctx)
{
        struct tc tc_white;

        for (int i = 0; i < ctx.prm.m; i++) {
                tc_white = {0};
                gen_tc(tc_white, WHITE, 500, 1000);
                v_tcs.push_back(tc_white);
        }
        sort_dec_tc_size(v_tcs);
        assign_ids(v_tcs);

        ctx.prm.m = v_tcs.size();

        for (unsigned int i = 0; i < v_tcs.size(); i++)
                v_tcs[i].gcd = gcd(v_tcs[i].v_tasks);
}

static void _gen_arch(vector<struct core> &v_cores, struct context &ctx)
{
        for (int i = 0; i < ctx.prm.m; i++)
                add_core(v_cores, RED, 1, ctx);
}

static void _store_results(vector<struct results> &v_res, struct results &res, 
                struct context &ctx_ffdu)
{
        res.m = ctx_ffdu.prm.m;
        res.et_part = ctx_ffdu.p.part_time * MSEC;
        res.et_assi = ctx_ffdu.p.assi_time * MSEC;
        res.et_schd = ctx_ffdu.p.schd_time * MSEC;
        res.et_swap = ctx_ffdu.p.swap_time * MSEC;
        res.et_disp = ctx_ffdu.p.disp_time * MSEC;
        res.et_tool = ctx_ffdu.p.exec_time * MSEC;
        v_res.push_back(res);
}

static void _write_results(vector<struct results> &v_res)
{
        FILE *file;

        vector<float> v_float;

        file = (FILE*)fopen(table, "w");

        write_header(file, hdr_table);

        for (unsigned int i = 0; i < v_res.size(); i++) {
                v_float.push_back(v_res[i].m);
                v_float.push_back(v_res[i].et_part);
                v_float.push_back(v_res[i].et_assi);
                v_float.push_back(v_res[i].et_schd);
                v_float.push_back(v_res[i].et_swap);
                v_float.push_back(v_res[i].et_disp);
                v_float.push_back(v_res[i].et_tool);
                write_data(file, v_float);
                v_float.clear();
        }
        fclose(file);
}

static void exp2(vector<struct results> &v_res)
{
        int m;
        struct results res;
        vector<struct core> v_cores;
        struct context ctx_ffdu;
        vector<struct core> v_cores_ffdu;
        vector<struct tc> v_tcs_ffdu;

        m = 0;

        for (unsigned int i = 0; i < ITR; i++) {
                m += STP;
redo:           res = {0};
                ctx_ffdu = {0};
                v_tcs_ffdu.clear();
                v_cores_ffdu.clear();

                ctx_ffdu.prm.m = m;
                ctx_ffdu.prm.a = FFDU;
                ctx_ffdu.prm.s = SIGMA;

                _gen_app(v_tcs_ffdu, ctx_ffdu);
                _gen_arch(v_cores_ffdu, ctx_ffdu);

                partitioning(v_tcs_ffdu, ctx_ffdu);

                assignment(v_tcs_ffdu, v_cores_ffdu, ctx_ffdu);
                if (STATE == FAILED) {
                        STATE = OK;
                        goto redo;
                }
                schedulability_analysis(v_cores_ffdu, ctx_ffdu);
                placement(v_cores_ffdu, ctx_ffdu);
                stats(v_cores_ffdu, v_tcs_ffdu, ctx_ffdu);
                _store_results(v_res, res, ctx_ffdu);
        }
        _write_results(v_res);
}

int main(void)
{
        vector<struct results> v_res;

        exp2(v_res);

        return 0;
}
