#ifndef GENERATOR_H
#define GENERATOR_H

#include <random>

#include "model.h"

/* precision for wcet generation */
#define EPSILON    100
#define PRECISION  0.10

/* number of task-chains */
#define MINN       1
#define MAXN       10000

/* permils */
#define MINMAXTU   1
#define MAXMAXTU   200

/* microsecs */
#define MINWCET    100
#define MAXWCET    25000

/* min max number of tasks in a chain */
#define MINTASKNBR 2
#define MAXTASKNBR 10

int gen_rand(int min, int max);

void input(int argc, char **argv, struct params &prm);

void init_ctx(vector<struct item> &v_itms, struct params &prm, 
                struct context &ctx);

void gen_tc_set(vector<struct item> &v_itms, struct params &prm, 
                struct context &ctx);

void partitioning(vector<struct item> &v_itms, struct context &ctx);

#endif /* GENERATOR_H */
