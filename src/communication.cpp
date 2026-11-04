#include "communication.h"

/* a fragment of a task-chain as found in a core after mapping */
struct frag {
        int tc_idx;
        int core;
        int ecu;
        int last_r;     /* WCRT of the last task of the fragment */
        int is_sched;   /* all tasks of the fragment schedulable */
};

static int _cmp_frag_tc_idx(const struct frag &a, const struct frag &b)
{
        return a.tc_idx < b.tc_idx;
}

/* collect, per original task-chain id, its fragments and where they reside */
static map<int, vector<struct frag>> _collect_frags(vector<struct core> &v_cores)
{
        map<int, vector<struct frag>> m_frags;

        for (unsigned int i = 0; i < v_cores.size(); i++) {
                for (unsigned int j = 0; j < v_cores[i].v_tcs.size(); j++) {
                        struct tc &tc = v_cores[i].v_tcs[j];
                        struct frag f;

                        if (tc.is_let == YES || tc.v_tasks.empty())
                                continue;

                        f.tc_idx = tc.tc_idx;
                        f.core = v_cores[i].id;
                        f.ecu = v_cores[i].color;
                        /* tasks of a fragment are in topological order */
                        f.last_r = tc.v_tasks.back().r;
                        f.is_sched = YES;
                        for (unsigned int k = 0; k < tc.v_tasks.size(); k++) {
                                if (tc.v_tasks[k].r > tc.v_tasks[k].t)
                                        f.is_sched = NO;
                        }
                        m_frags[tc.id].push_back(f);
                }
        }

        for (auto &kv : m_frags)
                sort(kv.second.begin(), kv.second.end(), _cmp_frag_tc_idx);

        return m_frags;
}

void comm_paths(vector<struct core> &v_cores, vector<struct tc_comm> &v_comm,
                struct context &ctx)
{
        map<int, vector<struct frag>> m_frags;

        v_comm.clear();
        ctx.intra_comm_count = 0;
        ctx.inter_comm_count = 0;

        m_frags = _collect_frags(v_cores);

        for (auto &kv : m_frags) {
                struct tc_comm com;
                vector<struct frag> &v_f = kv.second;

                com = {0};
                com.tc_id = kv.first;
                com.frag_count = v_f.size();

                for (unsigned int k = 0; k < v_f.size(); k++) {
                        if ((int)k != v_f[k].tc_idx) {
                                printf("ERR! TC %d fragment %d missing in cores\n",
                                                com.tc_id, k);
                                exit(0);
                        }

                        /* same core as the previous fragment: same segment */
                        if (k > 0 && v_f[k].core == com.v_segs.back().core) {
                                com.v_segs.back().last_tc_idx = v_f[k].tc_idx;
                                com.v_segs.back().r = v_f[k].last_r;
                                continue;
                        }

                        /* new core: hop from the previous segment */
                        if (k > 0) {
                                struct link lk;
                                struct segment &prev = com.v_segs.back();

                                lk = {0};
                                lk.src = prev.core;
                                lk.dst = v_f[k].core;
                                lk.src_ecu = prev.ecu;
                                lk.dst_ecu = v_f[k].ecu;
                                lk.src_tc_idx = prev.last_tc_idx;
                                lk.dst_tc_idx = v_f[k].tc_idx;

                                if (lk.src_ecu == lk.dst_ecu) {
                                        lk.comm_type = INTRA_ECU;
                                        ctx.intra_comm_count++;
                                } else {
                                        lk.comm_type = INTER_ECU;
                                        ctx.inter_comm_count++;
                                }
                                com.path.push_back(lk);
                        }

                        struct segment seg;
                        seg.core = v_f[k].core;
                        seg.ecu = v_f[k].ecu;
                        seg.first_tc_idx = v_f[k].tc_idx;
                        seg.last_tc_idx = v_f[k].tc_idx;
                        seg.r = v_f[k].last_r;
                        com.v_segs.push_back(seg);
                }
                v_comm.push_back(com);
        }
}

/*
 * Latency of a task-chain: the response times of the segments it goes
 * through, in order. Within a segment (consecutive fragments on one core)
 * all tasks are released together and precedence is enforced by priority,
 * so the WCRT of the last task covers the whole segment. At each hop the
 * output of the segment is available once its last task completes.
 * A non-fragmented task-chain has one segment: e2e = WCRT of its last task.
 */
void e2e_latency(vector<struct tc_comm> &v_comm, vector<struct core> &v_cores,
                struct context &ctx)
{
        map<int, vector<struct frag>> m_frags;

        ctx.e2e_ok_count = 0;
        ctx.e2e_failed_count = 0;

        m_frags = _collect_frags(v_cores);

        for (unsigned int i = 0; i < v_comm.size(); i++) {
                vector<struct frag> &v_f = m_frags[v_comm[i].tc_id];

                v_comm[i].e2e = 0;
                for (unsigned int j = 0; j < v_comm[i].v_segs.size(); j++)
                        v_comm[i].e2e += v_comm[i].v_segs[j].r;

                /* Rule 3.2 */
                v_comm[i].is_sched = YES;
                for (unsigned int j = 0; j < v_f.size(); j++) {
                        if (v_f[j].is_sched == NO)
                                v_comm[i].is_sched = NO;
                }

                if (v_comm[i].is_sched == YES)
                        ctx.e2e_ok_count++;
                else
                        ctx.e2e_failed_count++;
        }

        if (!v_comm.empty())
                ctx.p.sched_rate_tc = ((float)ctx.e2e_ok_count /
                                (float)v_comm.size()) * PERCENT;
}
