#ifndef LET_H
#define LET_H

#include "model.h"

void insert_let_tasks(vector<struct bin> &v_bins, struct context &ctx);

void update_let(struct bin &b, int gcd);

int check_if_fit_itm(struct bin &b, struct item &itm, int &gcd);

int check_if_fit_cut(struct bin &b, struct cut &c, int &gcd, int side);

#endif /* LET_H */
