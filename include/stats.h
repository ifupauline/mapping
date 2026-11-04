#ifndef STATS_H
#define STATS_H

#include "model.h"

struct results {
        float tc_count;
        float pa_count;
        float tk_count;

        float pa_win_rate;
        float ba_win_rate;
        float re_win_rate;

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

        /* communication (one algorithm) */
        float ic;       /* inter-core hops */
        float ie;       /* inter-ECU hops */
        float sr_tc;    /* task-chain schedulability rate (%) */
        float e2e_avg;  /* microsecs */
        float e2e_max;  /* microsecs */
        float et_comm;
        float et_comm_line;
        float et_comm_quad;

        /* communication (per algorithm) */
        float bfdu_ic;
        float wfdu_ic;
        float ffdu_ic;

        float bfdu_ie;
        float wfdu_ie;
        float ffdu_ie;

        float bfdu_sr_tc;
        float wfdu_sr_tc;
        float ffdu_sr_tc;

        float bfdu_e2e_avg;
        float wfdu_e2e_avg;
        float ffdu_e2e_avg;

        float bfdu_e2e_max;
        float wfdu_e2e_max;
        float ffdu_e2e_max;

        float bfdu_et_comm;
        float wfdu_et_comm;
        float ffdu_et_comm;

        /* TSN network (per algorithm): delay bounds (us), port load (%) */
        float bfdu_net_avg;
        float wfdu_net_avg;
        float ffdu_net_avg;

        float bfdu_net_max;
        float wfdu_net_max;
        float ffdu_net_max;

        float bfdu_net_util;
        float wfdu_net_util;
        float ffdu_net_util;

        /* TSN network (one algorithm) */
        float net_avg;
        float net_max;
        float net_util;
};

float sched_rate(vector<struct core> &v_cores, struct context &ctx);

void stats(vector<struct core> &v_cores, vector<struct tc> &v_tcs, 
                struct context &ctx);

#endif /* STATS_H */
