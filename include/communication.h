#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include "model.h"

void comp_comm(vector<struct bin> &v_bins, vector<struct item> &v_itms, 
                struct context &ctx);

void e2e_latency(vector<struct bin> &v_bins, vector<struct item> &v_itms, 
                struct context &ctx);

#endif /* COMMUNICATION_H */
