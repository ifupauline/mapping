#ifndef LET_H
#define LET_H

#include "model.h"

/* microsecs range based on paper Biondi and Di Natale */
#define MINLETWCET 1
#define MAXLETWCET 165

void init_let_task(struct tc &let, struct context &ctx);

void update_let(struct core &b, int gcd);

int check_if_fit_tc(struct core &b, struct tc &tc, int &gcd);
#endif /* LET_H */
