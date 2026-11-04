#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include "model.h"

/*
 * Path of every task-chain, core by core, with the hops between cores
 * (inter-core or inter-controller), and the communication counters.
 */
void comm_paths(vector<struct core> &v_cores, vector<struct tc_comm> &v_comm,
                struct context &ctx);

/*
 * End-to-end latency of every task-chain (needs comm_paths first) and
 * task-chain schedulability (Rule 3.2).
 */
void e2e_latency(vector<struct tc_comm> &v_comm, vector<struct core> &v_cores,
                struct context &ctx);

#endif /* COMMUNICATION_H */
