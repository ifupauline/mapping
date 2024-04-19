#ifndef STATS_H
#define STATS_H

#include "model.h"

struct results {
        int tc_count;
        int pa_count;
        int tk_count;

        float m;
        float fr;
        float sigma;
        float growth_rate;

        float et_part;
        float et_assi;
        float et_schd;
        float et_swap;
        float et_disp;
        float et_tool;

        float et_part_quad;
        float et_assi_quad;
        float et_schd_quad;
        float et_swap_quad;
        float et_disp_quad;
        float et_tool_quad;

        float et_part_line;
        float et_assi_line;
        float et_schd_line;
        float et_swap_line;
        float et_disp_line;
        float et_tool_line;

        float bfdu_fr;
        float wfdu_fr;
        float ffdu_fr;

        float bfdu_m;
        float wfdu_m;
        float ffdu_m;

        float bfdu_sr_allo;
        float wfdu_sr_allo;
        float ffdu_sr_allo;

        float bfdu_sr_swap;
        float wfdu_sr_swap;
        float ffdu_sr_swap;

        float bfdu_sr_disp;
        float wfdu_sr_disp;
        float ffdu_sr_disp;

        float bfdu_letu;
        float wfdu_letu;
        float ffdu_letu;

        float bfdu_swap_gain;
        float wfdu_swap_gain;
        float ffdu_swap_gain;

        float bfdu_disp_gain;
        float wfdu_disp_gain;
        float ffdu_disp_gain;

        float bfdu_plac_gain;
        float wfdu_plac_gain;
        float ffdu_plac_gain;

        float bfdu_et_tool;
        float wfdu_et_tool;
        float ffdu_et_tool;
};

float sched_rate(vector<struct core> &v_cores, struct context &ctx);

void stats(vector<struct core> &v_cores, vector<struct tc> &v_tcs, 
                struct context &ctx);

#endif /* STATS_H */
