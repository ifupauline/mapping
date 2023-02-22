#ifndef GENERATOR_H
#define GENERATOR_H

#include "mapping.h"

#define MINTASKNBR 2
#define MAXTASKNBR 15

#define MINWCET 1
#define MAXWCET 10

#define PERCENT 100

/* instance */
void gen_item_set(vector<struct item> &v_itms, struct params &prm);

void cmp_min_bins(vector<struct item> &v_itms, struct context &ctx);

void cmp_stats(vector<struct bin> &v_bins, vector<struct item> &v_itms, 
                struct context &ctx);

void init_ctx(struct params &prm, struct context &ctx);

void gen_tc_set(vector<struct item> &v_itms, struct params &prm);

#endif /* GENERATOR_H */
