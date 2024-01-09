#ifndef GENERATOR_H
#define GENERATOR_H

#include <random>

#include "model.h"

/* precision for wcet generation */
#define PRECISION  0.10

/* number of task-chains */
#define MINN       1
#define MAXN       10000

/* permils */
#define MINMAXTU   1
#define MAXMAXTU   100

/* microsecs */
#define MINWCET    1
#define MAXWCET    30000 /* 30 ms */

/* min max number of tasks in a chain */
#define MINTASKNBR 2
#define MAXTASKNBR 10

/* msg size */
#define SSIZE 0 /* 1   -> 250KB */
#define BSIZE 1 /* 250 -> 1000KB */

int gen_rand(int min, int max);

void input_case_study(int argc, char **argv, struct params &prm);

void init_ctx(vector<struct item> &v_itms, struct params &prm, 
                struct context &ctx);

void gen_case_study(vector<struct item> &v_itms, struct params &prm, 
                struct context &ctx);

void partitioning(vector<struct item> &v_itms, struct context &ctx);

#endif /* GENERATOR_H */
