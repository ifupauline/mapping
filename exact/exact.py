#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""
Exact assignment of the fragments to cores: the fewest cores such that
every core is schedulable, as in the heuristic method, on the instance
written by export_instance (same fragments, same constraints).

Model (CP-SAT, OR-Tools):
  x[f,p]  fragment f on core p (only where its controller and core type
          are allowed); every fragment on exactly one core;
  u[p]    core p used; x[f,p] <= u[p]; minimize the sum of u[p];
  a[p,g]  the LET task of core p has period g, g in the GCD closure of
          the fragment periods; one g per used core, and a fragment may
          only be on p if g divides the GCD of its periods;
  load    sum_f U[f,p] x[f,p] + sum_g ceil(C_let / g) a[p,g] <= phi u[p]
          (utilizations in ppm, rounded up as in the tool);
  memory  of each controller; replicas of a group on distinct controllers
          (or cores);
  symmetry: identical cores of a controller (same kind) are used in order,
          and the j-th fragment (in decreasing load) only goes to the first
          j + 1 of them.
Policy "centralized" (the placement rule of the heuristics): a free
fragment that may run on the central computer runs there, or on a zone
core next to a fragment of its own chain (affinity), or on a zone core if
a replica of its chain is on the central computer; "free" (default):
anywhere it is allowed.
Schedulability is checked outside the solver (lazy constraints): after a
solve, each used core is tested with the response-time analysis and the
precedence-preserving Audsley assignment of the tool; for an unschedulable
core, a set S of its fragments that stays unschedulable when any one is
removed is found and the cut sum_{f in S} x[f,q] <= |S| - 1 is added for
every core q of the same kind (a superset of an unschedulable set is
unschedulable, so the cut removes no feasible mapping). The solve is
repeated until every used core is schedulable.

The objective bound of the last solve is a lower bound of the optimum
(the cuts only remove infeasible mappings). The candidate cores are those
of export_instance (the architecture, with spare cores per controller);
"restricted" is reported if the solution uses every candidate of a
controller.

The assignment found (candidate core of each fragment) is in the output;
export_instance --verify checks it with the tool itself.

usage: exact.py <instance.json> [--time-limit s] [--workers n] [--json out]
                [--policy free|centralized]
"""
import argparse
import json
import math
import sys
import time

from ortools.sat.python import cp_model

UNIT = 1000000


def utilization(wcet, period):
    return (wcet * UNIT + period - 1) // period


def response_time(wcet, period, higher):
    """least fixed point of R = C + sum ceil(R / T_j) C_j, or period + 1"""
    r = wcet + sum(c for c, _ in higher)
    while True:
        nxt = wcet + sum(-(-r // t) * c for c, t in higher)
        if nxt > period:
            return period + 1
        if nxt == r:
            return r
        r = nxt


def schedulable(tasks, let_wcet):
    """tasks: (wcet, period, chain, id_in_chain) on one core; the LET task
    (let_wcet, gcd of the periods) has the highest priority. Audsley's
    assignment restricted to the orders where the tasks of one activation
    pattern (same chain, same period) keep decreasing priorities along the
    chain, as in src/sched.c: a schedulable order exists iff it finds one."""
    if not tasks:
        return True
    gcd = 0
    for _, t, _, _ in tasks:
        gcd = math.gcd(gcd, t)
    let = [(let_wcet, gcd)] if let_wcet and gcd else []
    if any(c > t for c, t, _, _ in tasks):
        return False
    # the next task of the same pattern
    nxt = {}
    patterns = {}
    for i, (_, t, chain, pos) in enumerate(tasks):
        patterns.setdefault((chain, t), []).append((pos, i))
    for members in patterns.values():
        members.sort()
        for (_, i), (_, j) in zip(members, members[1:]):
            nxt[i] = j
    left = set(range(len(tasks)))
    while left:
        chosen = None
        for i in sorted(left, key=lambda k: (-tasks[k][1], -k)):
            if nxt.get(i) in left:
                continue        # a later task of its pattern is still left
            higher = let + [(tasks[j][0], tasks[j][1]) for j in left if j != i]
            if response_time(tasks[i][0], tasks[i][1], higher) <= tasks[i][1]:
                chosen = i
                break
        if chosen is None:
            return False
        left.remove(chosen)
    return True


def gcd_closure(values):
    out = set(values)
    while True:
        new = {math.gcd(a, b) for a in out for b in out} - out
        if not new:
            return sorted(out)
        out |= new


class Instance:
    def __init__(self, data):
        self.data = data
        self.kinds = data["kinds"]
        self.cores = data["cores"]
        self.fragments = data["fragments"]
        self.phi = data["phi"]
        nf, nk = len(self.fragments), len(self.kinds)
        # utilization and GCD of each fragment on each kind
        self.util = [[sum(utilization(t["wcet"][k], t["period"])
                          for t in f["tasks"]) for k in range(nk)]
                     for f in self.fragments]
        self.gcd = []
        for f in self.fragments:
            g = 0
            for t in f["tasks"]:
                g = math.gcd(g, t["period"])
            self.gcd.append(g)
        self.periods = gcd_closure(set(self.gcd))
        self.allowed = [[p for p, core in enumerate(self.cores)
                         if f["allowed"] >> core["controller"] & 1 and
                         f["types"] >> self.kinds[core["kind"]]["type"] & 1]
                        for f in self.fragments]
        self.nf = nf

    def tasks_on(self, fragments, kind):
        return [(t["wcet"][kind], t["period"], self.fragments[f]["chain"],
                 t["id"]) for f in fragments for t in self.fragments[f]["tasks"]]

    def core_schedulable(self, fragments, kind):
        return schedulable(self.tasks_on(fragments, kind),
                           self.kinds[kind]["let_wcet"])


def build_model(inst, cuts, policy="free"):
    m = cp_model.CpModel()
    cores, frags = inst.cores, inst.fragments
    x = {}
    for f in range(inst.nf):
        for p in inst.allowed[f]:
            x[f, p] = m.NewBoolVar(f"x{f}_{p}")
        m.AddExactlyOne(x[f, p] for p in inst.allowed[f])
    central = set(inst.data["central"])
    if policy == "centralized":
        # a free fragment on a zone core only next to its own chain, or
        # if a replica of its chain holds the central computer
        pcu_replica = {}
        for f, fr in enumerate(frags):
            if fr["controller"] != inst.data["any"] or \
                    not any(fr["allowed"] >> c & 1 for c in central):
                continue
            siblings = [s for s, other in enumerate(frags)
                        if other["chain"] == fr["chain"] and s != f]
            reasons = []
            if fr["replica"] > 0:
                key = (fr["replica"], fr["chain"])
                if key not in pcu_replica:
                    others = [x[g, p] for g, other in enumerate(frags)
                              if other["replica"] == fr["replica"] and
                              other["chain"] != fr["chain"]
                              for p in inst.allowed[g]
                              if cores[p]["controller"] in central]
                    y = m.NewBoolVar("")
                    if others:
                        m.AddBoolOr(others).OnlyEnforceIf(y)
                    else:
                        m.Add(y == 0)
                    pcu_replica[key] = y
                reasons.append(pcu_replica[key])
            for p in inst.allowed[f]:
                if cores[p]["controller"] in central:
                    continue
                near = [x[s, p] for s in siblings if (s, p) in x] + reasons
                if near:
                    m.AddBoolOr(near).OnlyEnforceIf(x[f, p])
                else:
                    m.Add(x[f, p] == 0)
    u = [m.NewBoolVar(f"u{p}") for p in range(len(cores))]
    on = {p: [f for f in range(inst.nf) if (f, p) in x]
          for p in range(len(cores))}
    for p in range(len(cores)):
        for f in on[p]:
            m.AddImplication(x[f, p], u[p])
        if not on[p]:
            m.Add(u[p] == 0)
            continue
        kind = cores[p]["kind"]
        let = inst.kinds[kind]["let_wcet"]
        a = {g: m.NewBoolVar(f"a{p}_{g}") for g in inst.periods}
        m.Add(sum(a.values()) == u[p])
        for f in on[p]:
            m.Add(x[f, p] <= sum(a[g] for g in inst.periods
                                 if inst.gcd[f] % g == 0))
        m.Add(sum(inst.util[f][kind] * x[f, p] for f in on[p]) +
              sum(utilization(let, g) * a[g] for g in inst.periods)
              <= inst.phi * u[p])
    # memory of the controllers
    for c, capacity in enumerate(inst.data["ecu_memory"]):
        if capacity > 0:
            m.Add(sum(frags[f]["memory"] * x[f, p] for (f, p) in x
                      if cores[p]["controller"] == c) <= capacity)
    # replicas apart
    groups = {}
    for f, fr in enumerate(frags):
        if fr["replica"] > 0:
            groups.setdefault(fr["replica"], {}).setdefault(
                fr["chain"], []).append(f)
    by_cores = inst.data["separation"] == "cores"
    places = (range(len(cores)) if by_cores else
              sorted({core["controller"] for core in cores}))
    for chains in groups.values():
        if len(chains) < 2:
            continue
        for place in places:
            present = []
            for chain, fs in chains.items():
                xs = [x[f, p] for f in fs for p in inst.allowed[f]
                      if (p == place if by_cores
                          else cores[p]["controller"] == place)]
                if not xs:
                    continue
                y = m.NewBoolVar("")
                for v in xs:
                    m.AddImplication(v, y)
                present.append(y)
            if len(present) > 1:
                m.Add(sum(present) <= 1)
    # symmetry: identical cores of a controller
    same = {}
    for p, core in enumerate(cores):
        same.setdefault((core["controller"], core["kind"]), []).append(p)
    for members in same.values():
        for p, q in zip(members, members[1:]):
            m.AddImplication(u[q], u[p])
        for f in range(inst.nf):
            for b, p in enumerate(members):
                if b > f and (f, p) in x:
                    m.Add(x[f, p] == 0)
    # schedulability cuts
    for fragments, kind in cuts:
        for p, core in enumerate(cores):
            if core["kind"] != kind:
                continue
            if all((f, p) in x for f in fragments):
                m.Add(sum(x[f, p] for f in fragments) <= len(fragments) - 1)
    m.Minimize(sum(u))
    return m, x, u


def irreducible(inst, fragments, kind):
    """a subset that stays unschedulable, minimal for removing one"""
    fragments = list(fragments)
    i = 0
    while i < len(fragments):
        trial = fragments[:i] + fragments[i + 1:]
        if trial and not inst.core_schedulable(trial, kind):
            fragments = trial
        else:
            i += 1
    return fragments


def heuristic_hint(inst, name):
    """the assignment of a heuristic on the candidate cores (by controller
    and rank), if it fits them"""
    h = inst.data["heuristics"].get(name)
    if not h or not h["feasible"]:
        return None
    rank, seen = {}, {}
    for i, c in enumerate(h["core_controller"]):
        rank[i] = seen.get(c, 0)
        seen[c] = rank[i] + 1
    candidates = {}
    for p, core in enumerate(inst.cores):
        candidates.setdefault(core["controller"], []).append(p)
    hint = {}
    for f, core in enumerate(h["assignment"]):
        c = h["core_controller"][core]
        if rank[core] >= len(candidates[c]):
            return None
        hint[f] = candidates[c][rank[core]]
    return hint


def solve(inst, time_limit, workers, log=sys.stderr, policy="free"):
    cuts, start = [], time.time()
    hint = None
    for name in ("BFDU", "FFDU", "CADU", "WFDU"):
        hint = heuristic_hint(inst, name)
        if hint:
            break
    result = {"status": "UNKNOWN", "cores": None, "bound": None,
              "iterations": 0, "cuts": 0, "restricted": False}
    while True:
        left = time_limit - (time.time() - start)
        if left <= 0:
            break
        model, x, u = build_model(inst, cuts, policy)
        if hint:
            for (f, p), v in x.items():
                model.AddHint(v, hint.get(f) == p)
        solver = cp_model.CpSolver()
        solver.parameters.max_time_in_seconds = left
        solver.parameters.num_workers = workers
        status = solver.Solve(model)
        result["iterations"] += 1
        if status == cp_model.INFEASIBLE:
            result["status"] = "INFEASIBLE"
            break
        if status not in (cp_model.OPTIMAL, cp_model.FEASIBLE):
            break
        result["bound"] = max(result["bound"] or 0,
                              math.ceil(solver.BestObjectiveBound() - 1e-6))
        # schedulability of every used core
        new_cuts = []
        assignment = {}
        for (f, p), v in x.items():
            if solver.Value(v):
                assignment.setdefault(p, []).append(f)
        for p, fragments in assignment.items():
            kind = inst.cores[p]["kind"]
            if not inst.core_schedulable(fragments, kind):
                new_cuts.append((irreducible(inst, fragments, kind), kind))
        if not new_cuts:
            result["cores"] = int(round(solver.ObjectiveValue()))
            result["status"] = "OPTIMAL" if status == cp_model.OPTIMAL \
                else "FEASIBLE"
            used = {}
            for p in assignment:
                c = inst.cores[p]["controller"]
                used[c] = used.get(c, 0) + 1
            total = {}
            for core in inst.cores:
                total[core["controller"]] = total.get(core["controller"], 0) + 1
            result["restricted"] = any(used[c] == total[c] for c in used)
            where = [0] * inst.nf
            for p, fs in assignment.items():
                for f in fs:
                    where[f] = p
            result["assignment"] = where
            break
        cuts.extend(new_cuts)
        result["cuts"] = len(cuts)
        hint = {f: p for p, fs in assignment.items() for f in fs}
        print(f"  iteration {result['iterations']}: {len(new_cuts)} "
              f"unschedulable cores, {len(cuts)} cuts", file=log)
    result["time"] = time.time() - start
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[1])
    parser.add_argument("instance")
    parser.add_argument("--time-limit", type=float, default=60)
    parser.add_argument("--workers", type=int, default=2)
    parser.add_argument("--json")
    parser.add_argument("--policy", choices=("free", "centralized"),
                        default="free")
    args = parser.parse_args()
    with open(args.instance) as fh:
        data = json.load(fh)
    inst = Instance(data)
    # the checker agrees with the tool on the heuristic mappings
    for name, h in data["heuristics"].items():
        if not h["feasible"]:
            continue
        cores = {}
        for f, core in enumerate(h["assignment"]):
            cores.setdefault(core, []).append(f)
        kind_of = {}
        for p, core in enumerate(inst.cores):
            kind_of.setdefault(core["controller"], core["kind"])
        ok = all(inst.core_schedulable(fs, kind_of[h["core_controller"][c]])
                 for c, fs in cores.items())
        if ok != h["schedulable"]:
            print(f"warning: {name}: the checker says {ok}, the tool "
                  f"{h['schedulable']}", file=sys.stderr)
    result = solve(inst, args.time_limit, args.workers, policy=args.policy)
    result.update({"seed": data["seed"], "chains": data["chains"],
                   "fragments": inst.nf, "lower_bound": data["lower_bound"],
                   "heuristics": {k: v["cores"] if v["feasible"] else None
                                  for k, v in data["heuristics"].items()}})
    text = json.dumps(result)
    if args.json:
        with open(args.json, "w") as fh:
            fh.write(text + "\n")
    print(text)


if __name__ == "__main__":
    main()
