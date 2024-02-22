#ifndef UCASES_H
#define UCASES_H

#include "model.h"

#define DYNAMIC 0
#define STATIC  1

/* WATERS2019 AEB */
void create_waters2019(struct tc &tc);

/* WATERS2015 task periods */
int period_waters2015(int x, int y, int tc_type);

#endif /* UCASES_H */
