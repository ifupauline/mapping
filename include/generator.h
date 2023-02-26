#ifndef GENERATOR_H
#define GENERATOR_H

#include "mapping.h"

void init_ctx(vector<struct item> &v_itms, struct params &prm, 
                struct context &ctx);

void gen_tc_set(vector<struct item> &v_itms, struct params &prm, 
                struct context &ctx);

#endif /* GENERATOR_H */
