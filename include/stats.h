#ifndef STATS_H
#define STATS_H

#include "model.h"

struct results {
        float m;

        float et_part;
        float et_assi;
        float et_schd;
        float et_swap;
        float et_disp;
        float et_tool;

        float bfdu_fr;
        float wfdu_fr;
        float ffdu_fr;

        float sigma;
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

        float bfdu_plac_gain;
        float wfdu_plac_gain;
        float ffdu_plac_gain;
};

float sched_rate(vector<struct core> &v_cores, struct context &ctx);

void stats(vector<struct core> &v_cores, vector<struct tc> &v_tcs, 
                struct context &ctx);

#endif /* STATS_H */
