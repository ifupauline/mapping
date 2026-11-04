#ifndef NETWORK_H
#define NETWORK_H

#include "model.h"

/*
 * TSN NETWORK MODEL (inter-controller communication, Section 3.2.2.4)
 *
 * Topology: star. Each controller (PCU, ZCU1..5) has one full-duplex
 * Ethernet link to a central TSN switch, so a message from controller a to
 * controller b crosses two output ports: the egress port of a (its network
 * interface) and the switch egress port towards b.
 *
 * TSN features (IEEE 802.1Q): strict priority between two traffic classes.
 *  - LET class (highest priority, SR class A), shaped by the Credit-Based
 *    Shaper of IEEE 802.1Qav with idle slope I = NET_IDLE_SLOPE_PCT % of the
 *    link rate C, on every port.
 *  - best-effort class (lowest priority), assumed always present with
 *    maximum size frames, so a LET frame can be blocked by one of them
 *    (no frame preemption).
 *
 * Network Calculus (worst-case bounds):
 *  - each inter-ECU hop of a task-chain is a flow sending one message per
 *    period T of the producing task, released with jitter J (producer WCRT
 *    + source LET period): token bucket alpha(t) = b + r.J + r.t, b = bits
 *    of the message frames on the wire, r = b / T.
 *  - CBS service curve of the LET class on a port: rate-latency
 *    beta(t) = I.[t - T]^+ with T = (c_max - c_min) / I,
 *    c_max = I.L_be / C, c_min = -(C - I).L_let / C (credit bounds).
 *  - delay bound of a port: T + sum(bursts) / I, if sum(rates) <= I.
 *  - burst propagation: after a port with delay bound D, b' = b + r.D.
 */

#define NET_LINK_MBPS      1000 /* 1000BASE-T1 zonal backbone */
#define NET_IDLE_SLOPE_PCT 75   /* CBS idle slope, max allowed by 802.1Qav */
#define NET_SW_LAT_US      2    /* switch fabric + propagation latency */

/* Ethernet frame (802.1Q tagged) */
#define NET_MAX_PAYLOAD    1500
#define NET_MIN_PAYLOAD    42   /* 64 bytes minimum frame with VLAN tag */
#define NET_OVERHEAD       42   /* header 14 + tag 4 + FCS 4 + preamble 8 + IFG 12 */
#define NET_BE_FRAME       (NET_MAX_PAYLOAD + NET_OVERHEAD)

/* output message of a task (bytes), drawn by the generator */
#define MIN_MSG_BYTES      4
#define MAX_MSG_BYTES      256

/* bits on the wire for a message of payload bytes, and its largest frame */
int net_msg_bits(int payload, int &max_frame_bits);

/* bounds of every INTER_ECU link in v_comm (fills net_* fields) */
void net_analysis(vector<struct tc_comm> &v_comm, struct context &ctx);

#endif /* NETWORK_H */
