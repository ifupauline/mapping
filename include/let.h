#ifndef LET_H
#define LET_H

#include "model.h"

void insert_let_tasks(vector<struct bin> &v_bins, struct context &ctx);

void update_let(struct bin &b, int gcd);

int check_if_fit(struct bin &b, struct item &itm, struct context &ctx, int &gcd);

#endif /* LET_H */
