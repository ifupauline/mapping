#include "file.h"
#include "stats.h"
#include "generator.h"

#define SIG 150
#define TCC 5000 /* around 25000 tasks will be generated */

/* test the methodology on a large industrial size synthetic use case */
int EXP = EXP3;

static const char *synthetic  = "res_exp3.txt";
static const char *hdr_table =
"SIGMA, FR,"
"TC_COUNT, TASK_COUNT, PA_COUNT, ET_PART, ET_ASSI, ET_SWAP, ET_DISP, ET_TOOL,"
"IC, IE, SR_TC, E2E_AVG, E2E_MAX, ET_COMM";

static void _store_results(vector<struct results> &v_res, struct results &res, 
                struct context &ctx)
{
        res.sigma = (float)((float)SIG/PERMILL);
        res.fr = ctx.p.fr;
        res.tk_count = ctx.tasks_count;
        res.pa_count = ext_pa_count;
        res.tc_count = TCC;
        res.et_part = ctx.p.part_time * MSEC;
        res.et_assi = ctx.p.assi_time * MSEC;
        res.et_swap = ctx.p.swap_time * MSEC;
        res.et_disp = ctx.p.disp_time * MSEC;
        res.et_tool = ctx.p.exec_time * MSEC;
        res.ic = ctx.intra_comm_count;
        res.ie = ctx.inter_comm_count;
        res.sr_tc = ctx.p.sched_rate_tc;
        res.e2e_avg = ctx.p.e2e_avg;
        res.e2e_max = ctx.p.e2e_max;
        res.et_comm = ctx.p.comm_time * MSEC;
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
                v_float.push_back(v_res[i].fr);
                v_float.push_back(v_res[i].tc_count);
                v_float.push_back(v_res[i].tk_count);
                v_float.push_back(v_res[i].pa_count);
                v_float.push_back(v_res[i].et_part);
                v_float.push_back(v_res[i].et_assi);
                v_float.push_back(v_res[i].et_swap);
                v_float.push_back(v_res[i].et_disp);
                v_float.push_back(v_res[i].et_tool);
                v_float.push_back(v_res[i].ic);
                v_float.push_back(v_res[i].ie);
                v_float.push_back(v_res[i].sr_tc);
                v_float.push_back(v_res[i].e2e_avg);
                v_float.push_back(v_res[i].e2e_max);
                v_float.push_back(v_res[i].et_comm);
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

static void _synthetic(vector<struct results> &v_res)
{
        struct context ctx;
        struct results res;
        vector<struct core> v_cores;
        vector<struct tc> v_tcs;
        struct context ctx_wfdu;
        vector<struct core> v_cores_wfdu;
        vector<struct tc> v_tcs_wfdu;
        vector<struct tc_comm> v_comm_wfdu;

redo:
        ctx = {0};
        res = {0};

        v_tcs.clear();

        ctx.prm.m = TCC;
        _gen_app(v_tcs, ctx);

        ctx_wfdu = {0};
        ctx_wfdu = ctx;

        v_tcs_wfdu.clear();
        v_tcs_wfdu = v_tcs;

        v_cores_wfdu.clear();

        ctx_wfdu.prm.s = SIG;
        ctx_wfdu.prm.a = WFDU;

        partitioning(v_tcs_wfdu, ctx_wfdu);
        init_ctx(v_tcs_wfdu, ctx_wfdu.prm, ctx_wfdu);

        assignment(v_tcs_wfdu, v_cores_wfdu, ctx_wfdu);
        if (STATE == FAILED) {
                STATE = OK;
                goto redo;
        }

        schedulability_analysis(v_cores_wfdu, ctx_wfdu);
        placement(v_cores_wfdu, ctx_wfdu);
        communication(v_cores_wfdu, v_comm_wfdu, ctx_wfdu);
        stats(v_cores_wfdu, v_tcs_wfdu, ctx_wfdu);
        _store_results(v_res, res, ctx_wfdu);
        _write_results(v_res);
}

int main(void)
{
        vector<struct results> v_res_s;

        _synthetic(v_res_s);

        return 0;
}
