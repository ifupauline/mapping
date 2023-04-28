#ifndef LET_H
#define LET_H

#include "model.h"

void init_let_task(struct item &let, struct context &ctx);

void update_let(struct bin &b, int gcd);

int check_if_fit_itm(struct bin &b, struct item &itm, int &gcd);

int check_if_fit_cut(struct bin &b, struct cut &c, int &gcd, int side);

#endif /* LET_H */
