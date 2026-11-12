#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Tables of the paper (LaTeX), from the CSV files of the experiments.

usage: python3 make_tables.py [--results ../results] [--out sections]
       [--stats FILE]

Writes table_exact.tex, table_speed.tex, table_limit.tex, table_ablation.tex, table_practice.tex and
table_robustness.tex, and prints (or writes to FILE) the paired
differences quoted in the text: mean and 95 % confidence interval of the
difference to the reference point, instance by instance.
"""
import argparse
import csv
import math
import os
from collections import defaultdict

# Student t quantiles (0.975) for n - 1 degrees of freedom
T975 = {1: 12.706, 2: 4.303, 3: 3.182, 4: 2.776, 5: 2.571, 6: 2.447,
        7: 2.365, 8: 2.306, 9: 2.262, 10: 2.228, 11: 2.201, 12: 2.179,
        13: 2.160, 14: 2.145, 15: 2.131, 16: 2.120, 17: 2.110, 18: 2.101,
        19: 2.093, 20: 2.086}


def mean(values):
    return sum(values) / len(values) if values else math.nan


def paired(a, b):
    """mean and 95 % half-width (t) of b - a, paired by instance"""
    diffs = [b[k] - a[k] for k in a if k in b]
    n = len(diffs)
    m = mean(diffs)
    if n < 2:
        return m, 0.0, n
    sd = math.sqrt(sum((d - m) ** 2 for d in diffs) / (n - 1))
    return m, T975.get(n - 1, 1.96) * sd / math.sqrt(n), n


def load(results, name):
    with open(os.path.join(results, name + ".csv"), newline="") as handle:
        return [r for r in csv.DictReader(handle) if r["error"] == "0"]


def by_instance(rows, label, algorithm, key, feasible_only=True):
    out = {}
    for r in rows:
        if r["label"] == label and r["algorithm"] == algorithm and \
                (r["feasible"] == "1" or not feasible_only):
            out[int(r["instance"])] = float(r[key])
    return out


def on_time(r):
    return float(r["sr_on_time"])


def cell(rows, label, algorithm, key, fmt="{:.1f}"):
    values = list(by_instance(rows, label, algorithm, key).values())
    return fmt.format(mean(values)) if values else "--"


def write(out, name, text):
    with open(os.path.join(out, name), "w") as handle:
        handle.write(text)


# ---- exact formulation against the heuristics ----------------------------

EXACT_FILES = ("exact.csv", "exact_architectures.csv")


def table_exact(results, out, stats):
    rows = []
    for name in EXACT_FILES:
        path = os.path.join(results, name)
        if os.path.exists(path):
            with open(path, newline="") as handle:
                rows += list(csv.DictReader(handle))
    points = []
    for r in rows:
        key = (r["config"], int(r["chains"]))
        if key not in points:
            points.append(key)
    lines = []
    previous = None
    for config, n in points:
        ok = [r for r in rows if r["config"] == config and
              int(r["chains"]) == n and r["status"] in ("OPTIMAL", "FEASIBLE")]
        if not ok:
            continue
        if previous in ("constraints",) and config != previous:
            lines.append(r"\midrule")
        previous = config

        def gap(algorithm):
            values = [100 * (int(r[algorithm]) - int(r["exact"])) /
                      int(r["exact"]) for r in ok if r[algorithm] != ""]
            return f"{mean(values):.0f}"
        proven = sum(r["status"] == "OPTIMAL" for r in ok)
        lines.append(
            f"{config} & {n} & {proven}/{len(ok)} & "
            f"{mean([int(r['lower_bound']) for r in ok]):.1f} & "
            f"{mean([int(r['exact']) for r in ok]):.1f} & "
            f"{gap('BFDU')} / {gap('BFDU-')} & {gap('FFDU')} / {gap('FFDU-')} & "
            f"{gap('CADU')} / {gap('CADU-')} \\\\")
    every = [r for r in rows if r["status"] in ("OPTIMAL", "FEASIBLE")]
    for config in ("zonal", "domain", "hybrid"):
        mine = [r for r in every if (config == "zonal") ==
                (r["config"] in ("default", "slow zones", "constraints"))
                and (config == "zonal" or r["config"] == config)]
        for algorithm in ("BFDU", "FFDU", "CADU"):
            values = [100 * (int(r[algorithm]) - int(r["exact"])) /
                      int(r["exact"]) for r in mine if r[algorithm] != ""]
            optimal = sum(1 for r in mine if r[algorithm] != "" and
                          int(r[algorithm]) == int(r["exact"]))
            stats.append(f"exact {config:7} {algorithm} n={len(values)} "
                         f"gap {mean(values):.2f} % optimal {optimal} "
                         f"({100 * optimal / len(values):.1f} %)")
    write(out, "table_exact.tex", r"""\begin{table}[t]
\centering\footnotesize
\caption{Exact formulation against the heuristics (10 instances per row;
instances without an admissible mapping left out). Zonal platform:
default, slow zone controllers, spatial constraints; then the domain and
hybrid platforms of \cref{tab:templates}. \emph{proven}: solved to
optimality within 60\,s; \emph{opt.}: mean optimum (best found if not
proven); gaps in \% of the optimum, with consolidation / without.}
\label{tab:exact}
\setlength{\tabcolsep}{2pt}
\rowcolors{2}{brandlight}{white}
\begin{tabular}{@{}lrrrrrrr@{}}
\toprule
\rowcolor{white}
platform & chains & proven & $M_{\mathrm{LB}}$ & opt. & \BFDU & \FFDU &
\CADU \\
\midrule
""" + "\n".join(lines) + r"""
\bottomrule
\end{tabular}
\end{table}
""")


# ---- speed: exact against the heuristics and the metaheuristics ---------

def table_speed(results, out, stats):
    with open(os.path.join(results, "exact.csv"), newline="") as handle:
        exact = [r for r in csv.DictReader(handle)
                 if r["status"] in ("OPTIMAL", "FEASIBLE")]
    with open(os.path.join(results, "baselines.csv"), newline="") as handle:
        base = list(csv.DictReader(handle))
    sizes = [50, 100, 200, 300]
    lines = []
    for n in sizes:
        times = [float(r["time"]) for r in exact if int(r["chains"]) == n]
        proven = sum(1 for r in exact if int(r["chains"]) == n and
                     r["status"] == "OPTIMAL")

        def cpu(method, budget):
            return mean([float(r["cpu_s"]) for r in base
                         if int(r["chains"]) == n and r["method"] == method
                         and int(r["budget"]) == budget])
        lines.append(
            f"{n} & {mean(times):.2f} & {max(times):.1f} & "
            f"{proven}/{len(times)} & {1000 * cpu('FFDU', 0):.0f} & "
            f"{1000 * cpu('CADU', 0):.0f} & {cpu('SA', 1000000):.1f} & "
            f"{cpu('GA', 100):.1f} \\\\")
    write(out, "table_speed.tex", r"""\begin{table}[t]
\centering\footnotesize
\caption{Time to map one application. Exact: wall time of the constraint
program (two threads, 60\,s limit), mean and worst over the three
zonal platforms of \cref{tab:exact}, and the instances proven optimal. The
others: mean CPU time on the default platform (10 instances), the
heuristic with consolidation, the simulated annealing with $10^6$ moves and
the genetic algorithm with 100 generations.}
\label{tab:speed}
\setlength{\tabcolsep}{3pt}
\rowcolors{2}{brandlight}{white}
\begin{tabular}{@{}rrrrrrrr@{}}
\toprule
\rowcolor{white}
 & \multicolumn{3}{c}{exact (s)} & \multicolumn{2}{c}{heuristic (ms)} &
 \multicolumn{2}{c@{}}{baselines (s)} \\
\cmidrule(lr){2-4}\cmidrule(lr){5-6}\cmidrule(l){7-8}
\rowcolor{white}
chains & mean & worst & proven & \FFDU & \CADU & SA & GA \\
\midrule
""" + "\n".join(lines) + r"""
\bottomrule
\end{tabular}
\end{table}
""")


# ---- ablation ----------------------------------------------------------

ABLATION = [("full", "full method"), ("no affinity", "no affinity"),
            ("no consolidation", "no consolidation"),
            ("with swap moves", "with swap moves"),
            ("no relocation moves", "no relocation moves"),
            ("no local search", "no local search"),
            ("none", "none of them")]


def table_ablation(results, out, stats):
    rows = load(results, "ablation")
    lines = []
    for variant, name in ABLATION:
        cells = []
        for setting in ("default", "stressed"):
            label = f"{setting} {variant}"
            for algorithm in ("FFDU", "CADU"):
                cells.append(cell(rows, label, algorithm, "cores_used",
                                  "{:.2f}"))
                cells.append(cell(rows, label, algorithm, "sr_on_time"))
            cells.append(cell(rows, label, "FFDU", "sr_final"))
        lines.append(name + " & " + " & ".join(cells) + r" \\")
        for setting in ("default", "stressed"):
            for algorithm in ("FFDU", "CADU"):
                for key in ("cores_used", "sr_on_time", "hops_inter",
                            "sr_final"):
                    a = by_instance(rows, f"{setting} full", algorithm, key)
                    b = by_instance(rows, f"{setting} {variant}", algorithm,
                                    key)
                    m, h, n = paired(a, b)
                    stats.append(f"ablation {setting:8} {variant:20} "
                                 f"{algorithm} {key:11} {m:+8.2f} "
                                 f"+- {h:5.2f} (n={n})")
    write(out, "table_ablation.tex", r"""\begin{table*}[t]
\centering\footnotesize
\caption{Ablation: the method with one part removed (300 task-chains, 20
instances). Default: the default platform. Stressed: utilization cap
$\varphi=1$ and deadlines at 200\,\% of the sum of the pattern periods.
$M$: cores used; on time: task-chains that meet their deadline (\%);
sched.: cores schedulable at the end (\%, \FFDU).}
\label{tab:ablation}
\setlength{\tabcolsep}{4pt}
\rowcolors{2}{brandlight}{white}
\begin{tabular}{@{}lrrrrrrrrrr@{}}
\toprule
\rowcolor{white}
 & \multicolumn{5}{c}{default} & \multicolumn{5}{c@{}}{stressed} \\
\cmidrule(lr){2-6}\cmidrule(l){7-11}
\rowcolor{white}
 & \multicolumn{2}{c}{\FFDU} & \multicolumn{2}{c}{\CADU} & &
 \multicolumn{2}{c}{\FFDU} & \multicolumn{2}{c}{\CADU} & \\
\rowcolor{white}
variant & $M$ & on time & $M$ & on time & sched. & $M$ & on time & $M$ &
on time & sched. \\
\midrule
""" + "\n".join(lines) + r"""
\bottomrule
\end{tabular}
\end{table*}
""")


# ---- the sub-chain limit -------------------------------------------------

LIMIT_SETTINGS = [("default", "default"),
                  ("contention 50 ns", r"contention, 50\,ns"),
                  ("600 chains", "600 task-chains")]


def table_limit(results, out, stats):
    path = os.path.join(results, "limit.csv")
    if not os.path.exists(path):
        return
    with open(path, newline="") as handle:
        rows = [r for r in csv.DictReader(handle) if r["error"] == "0"]
    lines = []
    for key, name in LIMIT_SETTINGS:
        lines.append(r"\rowcolor{white}\multicolumn{8}{@{}l}{\emph{" + name +
                     r"}} \\")
        for sigma in ("0.4", "0.6", "0.8"):
            label = f"{key} sigma={sigma}"
            c = []
            for algorithm in ("FFDU", "CADU"):
                mine = [r for r in rows if r["label"] == label and
                        r["algorithm"] == algorithm]
                share = 100 * sum(r["feasible"] == "1" for r in mine) / len(mine)
                c += [f"{share:.0f}",
                      cell(rows, label, algorithm, "cores_used", "{:.2f}"),
                      cell(rows, label, algorithm, "hops_inter"),
                      cell(rows, label, algorithm, "sr_on_time")][:4 if algorithm == "CADU" else 4]
            lines.append(r"\quad $\sigma=" + sigma + "$ & " + " & ".join(c) +
                         r" \\")
            for algorithm in ("FFDU", "CADU"):
                for k in ("cores_used", "hops_inter", "sr_on_time"):
                    a = by_instance(rows, f"{key} sigma=0.4", algorithm, k)
                    b = by_instance(rows, label, algorithm, k)
                    m, h, n = paired(a, b)
                    stats.append(f"limit {key:16} sigma={sigma} {algorithm} "
                                 f"{k:11} {m:+8.2f} +- {h:5.2f} (n={n})")
    write(out, "table_limit.tex", r"""\begin{table}[t]
\centering\footnotesize
\caption{The sub-chain limit $\sigma$ (300 task-chains unless stated, 20
instances; $\varphi=0.8$). feas.: instances with an admissible mapping
(\%); $M$: cores used; hops: inter-ECU communications; on time: task-chains
that meet their deadline (\%); means over the feasible instances.}
\label{tab:limit}
\setlength{\tabcolsep}{2.1pt}
\begin{tabular}{@{}lrrrrrrrr@{}}
\toprule
 & \multicolumn{4}{c}{\FFDU} & \multicolumn{4}{c@{}}{\CADU} \\
\cmidrule(lr){2-5}\cmidrule(l){6-9}
 & feas. & $M$ & hops & on time & feas. & $M$ & hops & on time \\
\midrule
""" + "\n".join(lines).replace(r"\multicolumn{8}", r"\multicolumn{9}") + r"""
\bottomrule
\end{tabular}
\end{table}
""")


# ---- implicit communication or strict LET --------------------------------

COMMUNICATION_SETTINGS = [("default", "default point"),
                          ("deadline 500%", r"deadlines at 500\,\%"),
                          ("ring 3 classes", "ring, three classes")]


def table_communication(results, out, stats):
    path = os.path.join(results, "communication.csv")
    if not os.path.exists(path):
        return
    rows = load(results, "communication")
    lines = []
    for key, name in COMMUNICATION_SETTINGS:
        lines.append(r"\rowcolor{white}\multicolumn{7}{@{}l}{\emph{" + name +
                     r"}} \\")
        for scheme, label in (("implicit", "implicit"), ("LET", "strict LET")):
            c = []
            for algorithm in ("FFDU", "CADU"):
                point = f"{key} {scheme}"
                c += [cell(rows, point, algorithm, "sr_on_time"),
                      cell([dict(r, react_avg=str(float(r["react_avg"]) / 1000))
                            for r in rows], point, algorithm, "react_avg",
                           "{:.1f}"),
                      cell(rows, point, algorithm, "net_max", "{:.0f}")]
            lines.append(r"\quad " + label + " & " + " & ".join(c) + r" \\")
        for algorithm in ("FFDU", "CADU"):
            for k in ("sr_on_time", "react_avg", "net_avg", "net_max",
                      "cores_used"):
                a = by_instance(rows, f"{key} implicit", algorithm, k)
                b = by_instance(rows, f"{key} LET", algorithm, k)
                m, h, n = paired(a, b)
                stats.append(f"communication {key:16} LET-implicit {algorithm} "
                             f"{k:11} {m:+10.2f} +- {h:8.2f} (n={n})")
    write(out, "table_communication.tex", r"""\begin{table}[t]
\centering\footnotesize
\caption{Implicit communication or strict LET between cores (300
task-chains, 20 instances). on time: task-chains that meet their deadline
(\%); $\bar\Delta$: mean bound of the maximum reaction time (ms); $d$:
largest network bound (\si{\micro s}).}
\label{tab:communication}
\setlength{\tabcolsep}{2.6pt}
\begin{tabular}{@{}lrrrrrr@{}}
\toprule
 & \multicolumn{3}{c}{\FFDU} & \multicolumn{3}{c@{}}{\CADU} \\
\cmidrule(lr){2-4}\cmidrule(l){5-7}
 & on time & $\bar\Delta$ & $d$ & on time & $\bar\Delta$ & $d$ \\
\midrule
""" + "\n".join(lines) + r"""
\bottomrule
\end{tabular}
\end{table}
""")


# ---- metaheuristics on the hard platforms --------------------------------

BASELINE_PLATFORMS = [("slow", "slow zones", "exact.csv"),
                      ("constraints", "constraints", "exact.csv"),
                      ("domain", "domain", "exact_architectures.csv")]


def table_baselines_platforms(results, out, stats):
    path = os.path.join(results, "baselines_platforms.csv")
    if not os.path.exists(path):
        return
    with open(path, newline="") as handle:
        base = list(csv.DictReader(handle))
    lines = []
    for platform, config, exact_file in BASELINE_PLATFORMS:
        with open(os.path.join(results, exact_file), newline="") as handle:
            exact = {(r["chains"], r["seed"]): r for r in csv.DictReader(handle)
                     if r["config"] == config and
                     r["status"] in ("OPTIMAL", "FEASIBLE")}
        mine = [r for r in base if r["platform"] == platform]
        lines.append(r"\rowcolor{white}\multicolumn{10}{@{}l}{\emph{" + config +
                     r"}} \\")
        for n in (50, 100, 200, 300):
            keys = sorted({r["seed"] for r in mine if int(r["chains"]) == n
                           and (str(n), r["seed"]) in exact})
            if not keys:
                continue

            def row(method, budget, seed):
                for r in mine:
                    if (int(r["chains"]) == n and r["seed"] == seed and
                            r["method"] == method and int(r["budget"]) == budget):
                        return r
                return None

            methods = [("BFDU", 0), ("FFDU", 0), ("SA", 1000000), ("GA", 100)]
            # the reference: the best of the solver and of every method
            best = {}
            for seed in keys:
                e = int(exact[(str(n), seed)]["exact"])
                for m, b in methods:
                    r = row(m, b, seed)
                    if r and r["feasible"] == "1":
                        e = min(e, int(r["cores"]))
                best[seed] = e
            cells = []
            for m, b in methods:
                gaps = []
                for seed in keys:
                    r = row(m, b, seed)
                    if r and r["feasible"] == "1":
                        gaps.append((int(r["cores"]) - best[seed]) / best[seed] * 100)
                cells.append(f"{mean(gaps):.1f}" if len(gaps) == len(keys) else "--")
            for m, b in (("FFDU", 0), ("SA", 1000000)):
                v = [float(row(m, b, seed)["on_time_percent"]) for seed in keys
                     if row(m, b, seed)["feasible"] == "1"]
                cells.append(f"{mean(v):.1f}" if len(v) == len(keys) else "--")
            for m, b in (("SA", 1000000), ("GA", 100)):
                t = mean([float(row(m, b, seed)["cpu_s"]) for seed in keys])
                cells.append(f"{t:.0f}" if t >= 10 else f"{t:.1f}")
            lines.append(rf"\quad {n} & {len(keys)} & " + " & ".join(cells) + r" \\")
            a = {seed: int(row("FFDU", 0, seed)["cores"]) for seed in keys}
            b = {seed: int(row("SA", 1000000, seed)["cores"]) for seed in keys}
            m_, h_, k_ = paired(a, b)
            stats.append(f"baselines {platform:12} {n:4} SA-FFDU cores "
                         f"{m_:+7.2f} +- {h_:5.2f} (n={k_}); SA better on "
                         f"{sum(a[k] > b[k] for k in keys)}, worse on "
                         f"{sum(a[k] < b[k] for k in keys)}")
    write(out, "table_baselines_platforms.tex", r"""\begin{table}[t]
\centering\footnotesize
\caption{The heuristic against the metaheuristics on the hard platforms
of \cref{tab:exact} (10 instances per size; on the constrained platform,
no instance of 300 task-chains is feasible). Gap: cores above the best
allocation known (the exact solver or any method), \%, mean; on time:
task-chains that meet their deadline, \%, with the deadline local search
applied to every method; SA: $10^6$ moves, GA: 100 generations, with
their CPU times; --: no admissible mapping found on some instances (SA on
5 of the 10 constrained instances of 200 task-chains).}
\label{tab:baselines-platforms}
\setlength{\tabcolsep}{2.4pt}
\begin{tabular}{@{}lrrrrrrrrr@{}}
\toprule
 & & \multicolumn{4}{c}{gap (\%)} & \multicolumn{2}{c}{on time (\%)} & \multicolumn{2}{c@{}}{time (s)} \\
\cmidrule(lr){3-6}\cmidrule(lr){7-8}\cmidrule(l){9-10}
chains & inst. & \BFDU & \FFDU & SA & GA & \FFDU & SA & SA & GA \\
\midrule
""" + "\n".join(lines) + r"""
\bottomrule
\end{tabular}
\end{table}
""")


# ---- practitioner rules -------------------------------------------------

PRACTICE = [("method FFDU", "FFDU", r"method, \FFDU"),
            ("method CADU", "CADU", r"method, \CADU"),
            ("whole chains", "FFDU", r"whole task-chains"),
            ("central only", "FFDU", r"central computer only"),
            ("whole + central", "FFDU", r"whole and central only"),
            ("load balancing", "WFDU", r"load balancing (\WFDU)")]


def table_practice(results, out, stats):
    rows = load(results, "practice")
    lines = []
    for label, algorithm, name in PRACTICE:
        c = [cell(rows, label, algorithm, "cores_used", "{:.2f}"),
             cell(rows, label, algorithm, "cores_pcu", "{:.2f}") + "/" +
             cell(rows, label, algorithm, "cores_zcu", "{:.2f}"),
             cell(rows, label, algorithm, "hops_inter"),
             cell(rows, label, algorithm, "port_max"),
             cell(rows, label, algorithm, "sr_on_time")]
        lines.append(name + " & " + " & ".join(c) + r" \\")
        for key in ("cores_used", "hops_inter", "sr_on_time"):
            a = by_instance(rows, "method FFDU", "FFDU", key)
            b = by_instance(rows, label, algorithm, key)
            m, h, n = paired(a, b)
            stats.append(f"practice {label:16} {key:11} {m:+8.2f} "
                         f"+- {h:5.2f} (n={n})")
    write(out, "table_practice.tex", r"""\begin{table}[t]
\centering\footnotesize
\caption{The method against the rules of practice (300 task-chains, 20
instances). $M$: cores used, on the central computer / the zone
controllers; hops: inter-ECU communications; port: load of the busiest
port (\%); on time: task-chains that meet their deadline (\%).}
\label{tab:practice}
\setlength{\tabcolsep}{2.6pt}
\rowcolors{2}{brandlight}{white}
\begin{tabular}{@{}lrrrrr@{}}
\toprule
\rowcolor{white}
rule & $M$ & central/zones & hops & port & on time \\
\midrule
""" + "\n".join(lines) + r"""
\bottomrule
\end{tabular}
\end{table}
""")


# ---- robustness to the generator ---------------------------------------

ROBUSTNESS = [("default", "default", "--"),
              ("local", r"task-chains local to one zone (25\,\%)",
               ("local 0%", "local 50%")),
              ("sensors", r"start at a sensor / end at an actuator (60 / 50\,\%)",
               ("sensors 30%", "sensors 90%")),
              ("load", r"largest utilization of a task-chain (8\,\%)",
               ("chain load x0.5", "chain load x2")),
              ("heavy", r"heavy task-chains (2\,\%)",
               ("heavy 0%", "heavy 5%")),
              ("messages", r"messages above one frame (5\,\%)",
               ("messages small", "messages large"))]
VALUES = {"local 0%": r"0\,\%", "local 50%": r"50\,\%",
          "sensors 30%": r"30 / 25\,\%", "sensors 90%": r"90 / 80\,\%",
          "chain load x0.5": r"4\,\%", "chain load x2": r"16\,\%",
          "heavy 0%": r"0\,\%", "heavy 5%": r"5\,\%",
          "messages small": r"0\,\%", "messages large": r"20\,\%"}


def table_robustness(results, out, stats):
    rows = load(results, "robustness")
    lines = []

    def row(label, name):
        c = [cell(rows, label, "FFDU", "cores_lb", "{:.2f}")]
        for algorithm in ("FFDU", "CADU"):
            c += [cell(rows, label, algorithm, "cores_used", "{:.2f}"),
                  cell(rows, label, algorithm, "hops_inter"),
                  cell(rows, label, algorithm, "sr_on_time")]
        lines.append(name + " & " + " & ".join(c) + r" \\")
        for key in ("cores_used", "hops_inter", "sr_on_time"):
            a = by_instance(rows, label, "FFDU", key)
            b = by_instance(rows, label, "CADU", key)
            m, h, n = paired(a, b)
            stats.append(f"robustness {label:16} CADU-FFDU {key:11} "
                         f"{m:+8.2f} +- {h:5.2f} (n={n})")
    for key, name, labels in ROBUSTNESS:
        if key == "default":
            row("default", "default")
            continue
        lines.append(r"\rowcolor{white}\multicolumn{8}{@{}l}{\emph{" + name +
                     r"}} \\")
        for label in labels:
            row(label, r"\quad " + VALUES[label])
    write(out, "table_robustness.tex", r"""\begin{table}[t]
\centering\footnotesize
\caption{Robustness to the assumptions of the generator, one at a time
(default value in parentheses; 300 task-chains, 20 instances). LB: lower
bound on the cores; hops: inter-ECU communications; on time: task-chains
that meet their deadline (\%). Messages above one frame: 1.5 to 16\,kB;
without them, 90\,\% of the messages have 4 to 64\,B and 10\,\% one
frame.}
\label{tab:robustness}
\setlength{\tabcolsep}{2.2pt}
\begin{tabular}{@{}lrrrrrrr@{}}
\toprule
 & & \multicolumn{3}{c}{\FFDU} & \multicolumn{3}{c@{}}{\CADU} \\
\cmidrule(lr){3-5}\cmidrule(l){6-8}
assumption & LB & $M$ & hops & on time & $M$ & hops & on time \\
\midrule
""" + "\n".join(lines) + r"""
\bottomrule
\end{tabular}
\end{table}
""")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--results", default="../results")
    parser.add_argument("--out", default="sections")
    parser.add_argument("--stats", default=None)
    args = parser.parse_args()
    stats = []
    for make in (table_exact, table_speed, table_baselines_platforms,
                 table_limit, table_communication,
                 table_ablation, table_practice, table_robustness):
        make(args.results, args.out, stats)
    text = "\n".join(stats) + "\n"
    if args.stats:
        with open(args.stats, "w") as handle:
            handle.write(text)
    else:
        print(text, end="")


if __name__ == "__main__":
    main()
