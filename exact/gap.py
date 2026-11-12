#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""
Optimality gap of the heuristics: generated instances of increasing size,
each exported by export_instance (the fragments and the four heuristics,
with and without the consolidation), solved exactly by exact.py, the
exact assignment checked by the tool (export_instance --verify). Writes
results/exact.csv and prints a summary: the mean gap of each heuristic to
the optimum, in % of the optimum.

usage: gap.py <export_instance executable> [--sizes 20,40,...]
              [--seeds N] [--time-limit s] [--configs a,b]
              [--out results/exact.csv]
"""
import argparse
import csv
import json
import os
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import exact  # noqa: E402

HEURISTICS = ("BFDU", "WFDU", "FFDU", "CADU")
# with the consolidation, then without
VARIANTS = HEURISTICS + tuple(h + "-" for h in HEURISTICS)

# the default platform, slow zone controllers, the spatial constraints,
# the domain and the hybrid platforms
CONFIGS = {
    "default": [],
    "slow zones": ["--zcu-speed", "25"],
    "constraints": ["--replicas", "20", "--zone-only", "10",
                    "--zcu-memory", "8192"],
    "domain": ["--template", "domain"],
    "hybrid": ["--template", "hybrid"],
}


SPARE = []  # export_instance option: more candidate cores


def check(exporter, config, chains, seed, result, tmp):
    """the tool accepts the exact assignment, with as many cores"""
    if not result.get("assignment"):
        return
    assignment = os.path.join(tmp, "a.txt")
    with open(assignment, "w") as fh:
        fh.write(" ".join(map(str, result["assignment"])))
    out = subprocess.run([exporter, str(seed), str(chains), os.devnull,
                          "--verify", assignment] + CONFIGS[config] + SPARE,
                         capture_output=True, text=True)
    if out.returncode or out.stdout.strip() != f"valid {result['cores']}":
        sys.exit(f"{config} {chains} {seed}: the tool rejects the exact "
                 f"assignment: {out.stdout.strip()}")


def run(exporter, config, chains, seed, time_limit, workers, tmp):
    path = os.path.join(tmp, f"i_{chains}_{seed}.json")
    subprocess.run([exporter, str(seed), str(chains), path] + CONFIGS[config] +
                   SPARE,
                   check=True)
    with open(path) as fh:
        data = json.load(fh)
    row = {"config": config, "chains": chains, "seed": seed,
           "fragments": len(data["fragments"]),
           "lower_bound": data["lower_bound"]}
    for name in VARIANTS:
        h = data["heuristics"][name]
        row[name] = h["cores"] if h["feasible"] and h["schedulable"] else ""
    result = exact.solve(exact.Instance(data), time_limit, workers,
                         log=open(os.devnull, "w"))
    check(exporter, config, chains, seed, result, tmp)
    row.update({"status": result["status"], "exact": result["cores"],
                "bound": result["bound"],
                "restricted": int(result["restricted"]),
                "iterations": result["iterations"], "cuts": result["cuts"],
                "time": round(result["time"], 3)})
    return row


def mean(values):
    values = list(values)
    return sum(values) / len(values) if values else float("nan")


def summarize(rows):
    print(f"{'config':12} {'chains':>6} {'n':>3} {'proven':>7} {'time s':>6}"
          f" {'LB':>5} {'optimum':>7}" + "".join(f" {h:>10}" for h in
                                                 HEURISTICS))
    print("  mean gap to the optimum, with the consolidation / without")
    keys = sorted({(r["config"], r["chains"]) for r in rows},
                  key=lambda k: (list(CONFIGS).index(k[0]), k[1]))
    for config, chains in keys:
        group = [r for r in rows if r["config"] == config and
                 r["chains"] == chains and r["exact"] is not None]
        if not group:
            continue
        n = len(group)
        line = (f"{config:12} {chains:6d} {n:3d} "
                f"{100 * sum(r['status'] == 'OPTIMAL' for r in group) / n:6.0f}% "
                f"{mean(r['time'] for r in group):6.1f}"
                f" {mean(r['lower_bound'] for r in group):5.2f}"
                f" {mean(r['exact'] for r in group):7.2f}")
        for h in HEURISTICS:
            gaps = []
            for name in (h, h + "-"):
                mine = [r for r in group if r[name] != ""]
                gaps.append(mean((r[name] - r["exact"]) / r["exact"]
                                 for r in mine) * 100)
            line += f" {gaps[0]:4.0f}/{gaps[1]:3.0f}%"
        print(line)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("exporter")
    parser.add_argument("--sizes", default="20,40,60,80,100")
    parser.add_argument("--seeds", type=int, default=10)
    parser.add_argument("--time-limit", type=float, default=60)
    parser.add_argument("--workers", type=int, default=2)
    # the zonal platforms by default (results/exact.csv); the domain and
    # hybrid ones with --configs domain,hybrid
    # --out results/exact_architectures.csv
    parser.add_argument("--configs", default="default,slow zones,constraints")
    parser.add_argument("--out", default="results/exact.csv")
    # candidate cores per ECU beyond the most any heuristic uses (2)
    parser.add_argument("--spare", type=int, default=None)
    args = parser.parse_args()
    if args.spare is not None:
        SPARE[:] = ["--spare", str(args.spare)]
    rows = []
    with tempfile.TemporaryDirectory() as tmp:
        for config in args.configs.split(","):
            for chains in map(int, args.sizes.split(",")):
                for seed in range(1, args.seeds + 1):
                    row = run(args.exporter, config, chains, seed,
                              args.time_limit, args.workers, tmp)
                    rows.append(row)
                    print(f"  {config}, {chains} chains, seed {seed}: "
                          f"optimum {row['exact']} ({row['status']}, "
                          f"{row['time']:.1f} s); heuristics " +
                          " ".join(str(row[h]) for h in VARIANTS),
                          file=sys.stderr)
    os.makedirs(os.path.dirname(args.out) or ".", exist_ok=True)
    with open(args.out, "w", newline="") as fh:
        writer = csv.DictWriter(fh, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    summarize(rows)


if __name__ == "__main__":
    main()
