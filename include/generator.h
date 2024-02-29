#ifndef GENERATOR_H
#define GENERATOR_H

#include <random>

#include "model.h"

/* precision for wcet generation */
#define PRECISION  0.10

int gen_rand(int min, int max);

void input_prm(int argc, char **argv, struct params &prm);

void init_ctx(vector<struct tc> &v_tcs, struct params &prm, 
                struct context &ctx);

void gen_task(struct task &tau, int i, int color);

void gen_tc(struct tc &tc, int color, int minu, int maxu);

void gen_app(vector<struct tc> &v_tcs, struct params &prm, 
                struct context &ctx);
void gen_arch(vector<struct core> &v_cores, struct context &ctx);

void cut(vector<struct tc> &v_tcs, struct context &ctx);

#endif /* GENERATOR_H */
