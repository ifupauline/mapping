#ifndef OPTIMIZATION_H 
#define OPTIMIZATION_H

#include "mapping.h"

void reassignment(vector<struct bin> &v_bins, struct context &ctx);

void displacement(vector<struct bin> &v_bins, struct context &ctx);

void swapping(vector<struct bin> &v_bins, struct context &ctx);

#endif /* OPTIMIZATION_H */
