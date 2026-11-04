#include "network.h"

/* link rate and CBS idle slope in bits per microsecond */
static const double C = NET_LINK_MBPS;
static const double I = NET_LINK_MBPS * NET_IDLE_SLOPE_PCT / 100.0;

/* one message flow (one INTER_ECU hop of a task-chain) */
struct flow {
        struct link *lk;
        double b;       /* bits of one message on the wire */
        double l_max;   /* bits of its largest frame */
        double r;       /* rate b / T, bits per us */
        double burst;   /* burst of the arrival curve at the current port */
};

/* delay bound of the LET class on one output port and its utilization */
struct port {
        double sum_b;
        double sum_r;
        double l_max;
        double delay;
        int ok;
};

int net_msg_bits(int payload, int &max_frame_bits)
{
        int bits;
        int frame;
        int left;

        bits = 0;
        max_frame_bits = 0;
        left = payload < 1 ? 1 : payload;

        /* messages larger than one frame are sent as several frames */
        while (left > 0) {
                int p = left > NET_MAX_PAYLOAD ? NET_MAX_PAYLOAD : left;
                frame = ((p < NET_MIN_PAYLOAD ? NET_MIN_PAYLOAD : p) +
                                NET_OVERHEAD) * 8;
                bits += frame;
                if (frame > max_frame_bits)
                        max_frame_bits = frame;
                left -= p;
        }
        return bits;
}

/*
 * CBS rate-latency service curve beta(t) = I.[t - T]^+ of the highest
 * priority class, blocked by at most one best-effort frame:
 * T = (c_max - c_min) / I = L_be / C + (C - I).L_let / (C.I)
 */
static double _cbs_latency(double l_let_max)
{
        double l_be = NET_BE_FRAME * 8.0;

        return l_be / C + (C - I) * l_let_max / (C * I);
}

/* aggregate (FIFO) delay bound of the flows sharing one output port */
static void _port_bounds(map<int, struct port> &m_ports)
{
        for (auto &kv : m_ports) {
                struct port &pt = kv.second;

                pt.ok = pt.sum_r <= I ? YES : NO;
                pt.delay = pt.ok == YES ?
                        _cbs_latency(pt.l_max) + pt.sum_b / I : -1;
        }
}

void net_analysis(vector<struct tc_comm> &v_comm, struct context &ctx)
{
        int count;
        vector<struct flow> v_flows;
        map<int, struct port> m_src; /* egress port of each controller */
        map<int, struct port> m_sw;  /* switch port towards each controller */

        ctx.net_failed_count = 0;
        ctx.p.net_max = 0;
        ctx.p.net_avg = 0;
        ctx.p.net_util = 0;

        /* flows: arrival curves at the source controller */
        for (unsigned int i = 0; i < v_comm.size(); i++) {
                for (unsigned int j = 0; j < v_comm[i].path.size(); j++) {
                        struct link &lk = v_comm[i].path[j];
                        struct flow f;
                        int l_max;

                        if (lk.comm_type != INTER_ECU)
                                continue;

                        f.lk = &lk;
                        f.b = net_msg_bits(lk.msg_bytes, l_max);
                        f.l_max = l_max;
                        f.r = f.b / lk.msg_t;
                        f.burst = f.b + f.r * lk.msg_j;
                        v_flows.push_back(f);
                }
        }

        /* stage 1: egress port of the source controller */
        for (auto &f : v_flows) {
                struct port &pt = m_src[f.lk->src_ecu];
                pt.sum_b += f.burst;
                pt.sum_r += f.r;
                pt.l_max = max(pt.l_max, f.l_max);
        }
        _port_bounds(m_src);

        /* stage 2: switch egress port, bursts grown by the first port */
        for (auto &f : v_flows) {
                struct port &src = m_src[f.lk->src_ecu];
                struct port &pt = m_sw[f.lk->dst_ecu];

                if (src.ok == YES)
                        f.burst += f.r * src.delay;
                pt.sum_b += f.burst;
                pt.sum_r += f.r;
                pt.l_max = max(pt.l_max, f.l_max);
        }
        _port_bounds(m_sw);

        /* end-to-end network delay bound of each flow */
        count = 0;
        for (auto &f : v_flows) {
                struct port &src = m_src[f.lk->src_ecu];
                struct port &sw = m_sw[f.lk->dst_ecu];

                f.lk->net_src = src.delay;
                f.lk->net_sw = sw.delay;

                if (src.ok == NO || sw.ok == NO) {
                        f.lk->net_ok = NO;
                        f.lk->net_delay = -1;
                        ctx.net_failed_count++;
                        continue;
                }
                f.lk->net_ok = YES;
                f.lk->net_delay = src.delay + NET_SW_LAT_US + sw.delay;

                ctx.p.net_avg += f.lk->net_delay;
                ctx.p.net_max = max(ctx.p.net_max, f.lk->net_delay);
                count++;
        }
        if (count > 0)
                ctx.p.net_avg /= count;

        for (auto &kv : m_src)
                ctx.p.net_util = max(ctx.p.net_util, (float)(kv.second.sum_r / C * PERCENT));
        for (auto &kv : m_sw)
                ctx.p.net_util = max(ctx.p.net_util, (float)(kv.second.sum_r / C * PERCENT));
}
