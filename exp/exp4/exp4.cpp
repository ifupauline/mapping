#include "file.h"
#include "stats.h"
#include "print.h"
#include "generator.h"

#define ITR   6
#define STP   150

/* compare the time complexity analysis with the total execution time */
int EXP = EXP4;

static float growth_rate = 0;

static float prev_res_et_part = 0;
static float prev_res_et_assi = 0;
static float prev_res_et_swap = 0;
static float prev_res_et_disp = 0;
static float prev_res_et_tool = 0;

static float prev_res_et_part_quad = 0;
static float prev_res_et_assi_quad = 0;
static float prev_res_et_swap_quad = 0;
static float prev_res_et_disp_quad = 0;
static float prev_res_et_tool_quad = 0;

static float prev_res_et_part_line = 0;
static float prev_res_et_assi_line = 0;
static float prev_res_et_swap_line = 0;
static float prev_res_et_disp_line = 0;
static float prev_res_et_tool_line = 0;

static const char *table = "res_exp4.txt";
static const char *hdr_table = 
"m,"
"GROWTH_RATE,"
"ET_PART,"
"ET_PART_LINE,"
"ET_PART_QUAD,"
"ET_ASSI,"
"ET_ASSI_LINE,"
"ET_ASSI_QUAD,"
"ET_SWAP,"
"ET_SWAP_LINE,"
"ET_SWAP_QUAD,"
"ET_DISP,"
"ET_DISP_LINE,"
"ET_DISP_QUAD,"
"ET_TOOL,"
"ET_TOOL_LINE,"
"ET_TOOL_QUAD";

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

        ctx.prm.m = v_tcs.size();

        for (unsigned int i = 0; i < v_tcs.size(); i++)
                v_tcs[i].gcd = gcd(v_tcs[i].v_tasks);
}

static void _store_results(vector<struct results> &v_res, struct results &res, 
                struct context &ctx, int iter)
{
        res.m = ctx.prm.m;
        res.growth_rate = growth_rate;
        res.et_part = ctx.p.part_time * (MSEC * 10); /* to be above 0 */
        res.et_assi = ctx.p.assi_time * MSEC;
        res.et_swap = ctx.p.swap_time * MSEC;
        res.et_disp = ctx.p.disp_time * MSEC;
        res.et_tool = ctx.p.exec_time * MSEC;

        if (iter == 0) {
                res.growth_rate = 1;
                res.et_part_quad = res.et_part;
                res.et_assi_quad = res.et_assi;
                res.et_swap_quad = res.et_swap;
                res.et_disp_quad = res.et_disp;
                res.et_tool_quad = res.et_tool;

                res.et_part_line = res.et_part;
                res.et_assi_line = res.et_assi;
                res.et_swap_line = res.et_swap;
                res.et_disp_line = res.et_disp;
                res.et_tool_line = res.et_tool;

                prev_res_et_part_quad = res.et_part_quad;
                prev_res_et_assi_quad = res.et_assi_quad;
                prev_res_et_swap_quad = res.et_swap_quad;
                prev_res_et_disp_quad = res.et_disp_quad;
                prev_res_et_tool_quad = res.et_tool_quad;

                prev_res_et_part_line = res.et_part_line;
                prev_res_et_assi_line = res.et_assi_line;
                prev_res_et_swap_line = res.et_swap_line;
                prev_res_et_disp_line = res.et_disp_line;
                prev_res_et_tool_line = res.et_tool_line;
                v_res.push_back(res);
                return;
        }

        res.et_part_line = prev_res_et_part_line * growth_rate;
        res.et_assi_line = prev_res_et_assi_line * growth_rate;
        res.et_swap_line = prev_res_et_swap_line * growth_rate;
        res.et_disp_line = prev_res_et_disp_line * growth_rate;
        res.et_tool_line = prev_res_et_tool_line * growth_rate;

        res.et_part_quad = prev_res_et_part_quad * growth_rate * 2;
        res.et_assi_quad = prev_res_et_assi_quad * growth_rate * 2;
        res.et_swap_quad = prev_res_et_swap_quad * growth_rate * 2;
        res.et_disp_quad = prev_res_et_disp_quad * growth_rate * 2;
        res.et_tool_quad = prev_res_et_tool_quad * growth_rate * 2;

        /* save previous execution time */
        prev_res_et_part_line = res.et_part_line;
        prev_res_et_assi_line = res.et_assi_line;
        prev_res_et_swap_line = res.et_swap_line;
        prev_res_et_disp_line = res.et_disp_line;
        prev_res_et_tool_line = res.et_tool_line;

        prev_res_et_part_quad = res.et_part_quad;
        prev_res_et_assi_quad = res.et_assi_quad;
        prev_res_et_swap_quad = res.et_swap_quad;
        prev_res_et_disp_quad = res.et_disp_quad;
        prev_res_et_tool_quad = res.et_tool_quad;

        v_res.push_back(res);
}

static void _write_results(vector<struct results> &v_res)
{
        printf("\nWriting Results to %s\n", table);

        FILE *file;

        vector<float> v_float;

        file = (FILE*)fopen(table, "w");

        write_header(file, hdr_table);

        for (unsigned int i = 0; i < v_res.size(); i++) {
                v_float.push_back(v_res[i].m);
                v_float.push_back(v_res[i].growth_rate);
                v_float.push_back(v_res[i].et_part);
                v_float.push_back(v_res[i].et_part_line);
                v_float.push_back(v_res[i].et_part_quad);
                v_float.push_back(v_res[i].et_assi);
                v_float.push_back(v_res[i].et_assi_line);
                v_float.push_back(v_res[i].et_assi_quad);
                v_float.push_back(v_res[i].et_swap);
                v_float.push_back(v_res[i].et_swap_line);
                v_float.push_back(v_res[i].et_swap_quad);
                v_float.push_back(v_res[i].et_disp);
                v_float.push_back(v_res[i].et_disp_line);
                v_float.push_back(v_res[i].et_disp_quad);
                v_float.push_back(v_res[i].et_tool);
                v_float.push_back(v_res[i].et_tool_line);
                v_float.push_back(v_res[i].et_tool_quad);
                write_data(file, v_float);
                v_float.clear();
        }
        fclose(file);
}

static void _synthetic(vector<struct results> &v_res)
{
        int m;
        float prev;
        struct results res;
        struct context ctx_wfdu;
        vector<struct core> v_cores;
        vector<struct core> v_cores_wfdu;
        vector<struct tc> v_tcs_wfdu;

        m = 0;
        prev = 0;

        for (unsigned int i = 0; i < ITR; i++) {
                m += STP;
redo:           res = {0};
                ctx_wfdu = {0};
                v_tcs_wfdu.clear();
                v_cores_wfdu.clear();

                ctx_wfdu.prm.m = m;
                ctx_wfdu.prm.a = WFDU;
                ctx_wfdu.prm.s = 150;

                _gen_app(v_tcs_wfdu, ctx_wfdu);

                partitioning(v_tcs_wfdu, ctx_wfdu);

                assignment(v_tcs_wfdu, v_cores_wfdu, ctx_wfdu);
                if (STATE == FAILED) {
                        STATE = OK;
                        goto redo;
                }
                schedulability_analysis(v_cores_wfdu, ctx_wfdu);
                placement(v_cores_wfdu, ctx_wfdu);
                stats(v_cores_wfdu, v_tcs_wfdu, ctx_wfdu);

                if (i == 0) {
                        prev_res_et_part = ctx_wfdu.p.part_time * MSEC;
                        prev_res_et_assi = ctx_wfdu.p.assi_time * MSEC;
                        prev_res_et_swap = ctx_wfdu.p.swap_time * MSEC;
                        prev_res_et_disp = ctx_wfdu.p.disp_time * MSEC;
                        prev_res_et_tool = ctx_wfdu.p.exec_time * MSEC;
                }
                prev = m - STP;
                growth_rate = m/prev;

                _store_results(v_res, res, ctx_wfdu, i);
        }
        _write_results(v_res);
}

int main(void)
{
        vector<struct results> v_res;

        _synthetic(v_res);

        return 0;
}
