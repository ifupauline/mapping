#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""
Full-system simulation of a mapping: the tasks on their cores, the LET
communication between cores and the network, to check that the
end-to-end delay and the maximum reaction time of every task-chain never
exceed the bounds of the analysis (src/communication.c).

Input: the system.json written by "mapping --out DIR": for every core, its
LET task (period, WCET) and its tasks (period, WCET on that core,
priority); for every task-chain, the core of each task, the network
bound of each hop and the two bounds.

The model simulated, as the analysis assumes it:
 - every core runs its jobs under preemptive fixed priorities; its LET task
   has the highest priority, its jobs and those of its tasks are released
   at multiples of their periods from a phase of the core, drawn in
   [0, LET period); the cores are not synchronized;
 - a job reads its inputs when it starts running (implicit communication)
   and writes its output when it ends;
 - two consecutive tasks of a chain on one core communicate through a
   register: the consumer reads the last output written;
 - between cores, the LET task of the producer core copies the last
   output written by its release (implicit communication) or, with strict
   LET ("communication": "LET"), the output of the last producer job whose
   period has ended by then; its copy is sent when it ends:
   over the network (an inter-ECU hop, a delay at most the network bound,
   messages in order) or through the shared memory of the ECU (no delay);
   the LET task of the consumer core reads, at the release of the consumer
   job, the last message arrived by then;
 - the first task of a chain samples its sensor when its job starts.
A sample is followed through the chain by its timestamp. For the samples
s_k of the first task, F(s_k) is the end of the first job of the last task
whose output comes from a sample at least as recent. Observed end-to-end
delay: F(s_k) - r_k (r_k the release of the sampling job); observed
maximum reaction time: F(s_k) - s_(k-1) (an event just after the previous
sample waits for s_k).

Scenarios: execution times at the WCET and network delays at the bound,
or execution times uniform in [0.5 C, C] and delays uniform in [0, bound];
every scenario draws new phases. Output: one line per run of the file,
with the largest observed/bound ratios, and "violations = N".

Usage: syssim.py <system.json> [scenarios (4)] [seed (1)] [ratios.csv]
(ratios.csv: one row per run and chain with the bounds, the largest
observed values over the scenarios and the inter-ECU hops of the chain)
"""
import bisect
import heapq
import json
import math
import random
import sys


def simulate_core(core, horizon, phase, worst, rng):
    """the jobs of every task of @core: {(chain, task) or 'LET': [jobs]},
    a job being [release, start, finish]"""
    releases = []
    seq = 0
    tasks = [("LET", 1, core["let_period_us"], core["let_wcet_us"])]
    for t in core["tasks"]:
        tasks.append(((t["chain"], t["task"]), t["priority"], t["period_us"],
                      t["wcet_us"]))
    jobs = {}
    for key, priority, period, wcet in tasks:
        jobs[key] = []
        if period <= 0:
            continue
        k = 0
        while phase + k * period < horizon:
            c = wcet if worst else rng.uniform(0.5 * wcet, wcet)
            job = [phase + k * period, None, None]
            jobs[key].append(job)
            releases.append((job[0], priority, seq, job, c))
            seq += 1
            k += 1
    releases.sort(key=lambda r: (r[0], r[1], r[2]))
    ready, i, t = [], 0, 0.0
    while i < len(releases) or ready:
        if not ready:
            t = max(t, releases[i][0])
        while i < len(releases) and releases[i][0] <= t:
            rel = releases[i]
            heapq.heappush(ready, [rel[1], rel[0], rel[2], rel[3], rel[4]])
            i += 1
        top = ready[0]
        job = top[3]
        if job[1] is None:
            job[1] = t
        next_release = releases[i][0] if i < len(releases) else math.inf
        run = min(top[4], next_release - t)
        t += run
        top[4] -= run
        if top[4] <= 1e-9:
            job[2] = t
            heapq.heappop(ready)
    return jobs


def latest(times, values, at):
    """the value of the last time <= @at (times sorted), or None"""
    k = bisect.bisect_right(times, at + 1e-9) - 1
    return values[k] if k >= 0 else None


def check_run(run, scenarios, rng, rows):
    cores = {c["id"]: c for c in run["cores"]}
    strict = run.get("communication") == "LET"
    period = {(c["id"], t["chain"], t["task"]): t["period_us"]
              for c in run["cores"] for t in c["tasks"]}
    chains = [c for c in run["chains"] if c["bounded"]]
    periods = [t["period_us"] for c in run["cores"] for t in c["tasks"]]
    horizon = 3 * max(periods) + 2 * max(c["reaction_bound_us"]
                                         for c in chains) if chains else 0
    worst_ratio = [0.0, 0.0]
    violations = 0
    largest = {c["id"]: [0.0, 0.0] for c in chains}
    for scenario in range(scenarios):
        worst = scenario % 2 == 0
        phases = {i: rng.uniform(0, max(c["let_period_us"], 1))
                  for i, c in cores.items()}
        sim = {i: simulate_core(c, horizon, phases[i], worst, rng)
               for i, c in cores.items()}
        for chain in chains:
            tasks = chain["tasks"]
            first = sim[tasks[0]["core"]][(chain["id"], tasks[0]["task"])]
            # data of the first task: its start time; (finish, stamp,
            # release) of every job
            out = [(j[2], j[1], j[0]) for j in first if j[2] is not None]
            samples = [(j[0], j[1]) for j in first if j[1] is not None]
            for k in range(1, len(tasks)):
                prev, cur = tasks[k - 1], tasks[k]
                jobs = sim[cur["core"]][(chain["id"], cur["task"])]
                out.sort()
                times = [o[0] for o in out]
                stamps = [o[1] for o in out]
                if prev["core"] == cur["core"]:
                    # register: read at start
                    available = (times, stamps)
                    read = 1
                else:
                    # LET copy on the producer core, then the hop
                    lets = sim[prev["core"]]["LET"]
                    bound = prev["hop_bound_us"]
                    if strict:
                        # published at the end of the producer's period
                        t_prev = period[(prev["core"], chain["id"],
                                         prev["task"])]
                        ends = sorted((o[2] + t_prev, o[1]) for o in out)
                        times = [e[0] for e in ends]
                        stamps = [e[1] for e in ends]
                    arrivals, values, last = [], [], -math.inf
                    for let in lets:
                        if let[2] is None:
                            continue
                        v = latest(times, stamps, let[0])
                        if v is None:
                            continue
                        d = bound if worst else rng.uniform(0, bound)
                        last = max(last, let[2] + d)  # in order
                        arrivals.append(last)
                        values.append(v)
                    available = (arrivals, values)
                    read = 0
                out = []
                for j in jobs:
                    if j[2] is None or j[1] is None:
                        continue
                    v = latest(available[0], available[1], j[read])
                    if v is not None:
                        out.append((j[2], v, j[0]))
            out.sort()
            finish = [o[0] for o in out]
            best, prefix = -math.inf, []
            for o in out:
                best = max(best, o[1])
                prefix.append(best)
            observed = [0.0, 0.0]
            for k in range(1, len(samples)):
                s = samples[k][1]
                m = bisect.bisect_left(prefix, s - 1e-9)
                if m == len(prefix):
                    break
                f = finish[m]
                observed[0] = max(observed[0], f - samples[k][0])
                observed[1] = max(observed[1], f - samples[k - 1][1])
            bounds = (chain["end_to_end_bound_us"], chain["reaction_bound_us"])
            for x in range(2):
                largest[chain["id"]][x] = max(largest[chain["id"]][x],
                                              observed[x])
                if observed[x] > bounds[x] + 1e-6:
                    violations += 1
                    print(f"  chain {chain['id']}: observed "
                          f"{observed[x]:.1f} > bound {bounds[x]} "
                          f"({'end-to-end' if x == 0 else 'reaction'})")
                if bounds[x] > 0:
                    worst_ratio[x] = max(worst_ratio[x],
                                         observed[x] / bounds[x])
    for chain in chains:
        tasks = chain["tasks"]
        hops = sum(1 for a, b in zip(tasks, tasks[1:])
                   if cores[a["core"]]["ecu"] != cores[b["core"]]["ecu"])
        rows.append((run["allocation"], chain["id"], len(tasks), hops,
                     chain["end_to_end_bound_us"], largest[chain["id"]][0],
                     chain["reaction_bound_us"], largest[chain["id"]][1]))
    return len(chains), worst_ratio, violations


def main():
    path = sys.argv[1]
    scenarios = int(sys.argv[2]) if len(sys.argv) > 2 else 4
    rng = random.Random(int(sys.argv[3]) if len(sys.argv) > 3 else 1)
    with open(path) as fh:
        system = json.load(fh)
    total, rows = 0, []
    for run in system["runs"]:
        if not run["feasible"]:
            continue
        count, ratio, violations = check_run(run, scenarios, rng, rows)
        total += violations
        print(f"{path} {run['allocation']}: {count} chains, {scenarios} "
              f"scenarios, max observed/bound: end-to-end {ratio[0]:.3f}, "
              f"reaction {ratio[1]:.3f}, violations = {violations}")
    if len(sys.argv) > 4:
        with open(sys.argv[4], "w") as fh:
            fh.write("allocation,chain,tasks,inter_ecu_hops,"
                     "end_to_end_bound_us,end_to_end_observed_us,"
                     "reaction_bound_us,reaction_observed_us\n")
            for row in rows:
                fh.write("%s,%d,%d,%d,%d,%.3f,%d,%.3f\n" % row)
    return 1 if total else 0


if __name__ == "__main__":
    sys.exit(main())
