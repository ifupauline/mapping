#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Figures of the paper, from the CSV files of `experiment all`.

usage: python3 make_figures.py [--results ../results] [--out figures]

Every figure shows means over the instances with their 95 % confidence
interval; one colour and one marker per assignment algorithm (fixed order,
colour-vision-deficiency safe palette), so that no series depends on colour
alone.
"""
import argparse
import csv
import math
import os
from collections import defaultdict

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
from matplotlib.lines import Line2D  # noqa: E402

ALGORITHMS = ["BFDU", "WFDU", "FFDU", "CADU"]
COLOURS = {"BFDU": "#2a78d6", "WFDU": "#eb6834", "FFDU": "#1baf7a",
           "CADU": "#eda100"}
MARKERS = {"BFDU": "o", "WFDU": "s", "FFDU": "^", "CADU": "D"}
HATCHES = {"BFDU": "", "WFDU": "////", "FFDU": "....", "CADU": "xxxx"}
INK, MUTED, GRID = "#1b1b1b", "#5c5b57", "#e4e3df"
COLUMN, PAGE = 3.37, 7.0          # inches


def style():
    matplotlib.rcParams.update({
        "font.family": "sans-serif",
        "font.sans-serif": ["Liberation Sans", "DejaVu Sans"],
        "mathtext.fontset": "stixsans",
        "font.size": 7.5, "axes.titlesize": 7.5, "axes.labelsize": 7.5,
        "legend.fontsize": 7, "xtick.labelsize": 7, "ytick.labelsize": 7,
        "axes.edgecolor": MUTED, "axes.labelcolor": INK,
        "xtick.color": MUTED, "ytick.color": MUTED,
        "axes.grid": True, "grid.color": GRID, "grid.linewidth": 0.5,
        "axes.spines.top": False, "axes.spines.right": False,
        "axes.linewidth": 0.6, "xtick.major.width": 0.6,
        "ytick.major.width": 0.6, "lines.linewidth": 1.3,
        "lines.markersize": 4, "legend.frameon": False,
        "pdf.fonttype": 42, "savefig.bbox": "tight",
        "savefig.pad_inches": 0.02, "axes.titleweight": "semibold",
        "axes.titlecolor": INK, "axes.titlelocation": "left",
    })


def load(folder, name):
    rows = []
    with open(os.path.join(folder, name + ".csv"), newline="") as handle:
        for row in csv.DictReader(handle):
            for key, value in row.items():
                if key in ("label", "algorithm", "application", "experiment",
                           "topology", "shaping", "separation"):
                    continue
                try:
                    row[key] = float(value)
                except ValueError:
                    pass
            if row["error"] == 0:
                rows.append(row)
    return rows


def mean_ci(values):
    values = [v for v in values if not math.isnan(v)]
    if not values:
        return math.nan, 0.0
    mean = sum(values) / len(values)
    if len(values) < 2:
        return mean, 0.0
    var = sum((v - mean) ** 2 for v in values) / (len(values) - 1)
    return mean, 1.96 * math.sqrt(var / len(values))


def series(rows, x_key, y, select, feasible_only=True):
    groups = defaultdict(list)
    for row in rows:
        if select(row) and (row["feasible"] or not feasible_only):
            groups[row[x_key]].append(y(row))
    xs = sorted(groups)
    stats = [mean_ci(groups[x]) for x in xs]
    return xs, [s[0] for s in stats], [s[1] for s in stats]


def line(axis, xs, means, cis, algorithm, label=None, dashed=False):
    colour = COLOURS[algorithm]
    axis.plot(xs, means, color=colour, marker=MARKERS[algorithm],
              linestyle="--" if dashed else "-",
              label=label if label is not None else algorithm,
              markeredgecolor="white", markeredgewidth=0.5)
    axis.fill_between(xs, [m - c for m, c in zip(means, cis)],
                      [m + c for m, c in zip(means, cis)], color=colour,
                      alpha=0.15, linewidth=0)


def per_algorithm(axis, rows, x_key, y, feasible_only=True):
    for algorithm in ALGORITHMS:
        xs, means, cis = series(rows, x_key, y,
                                lambda r, a=algorithm: r["algorithm"] == a,
                                feasible_only)
        if xs:
            line(axis, xs, means, cis, algorithm)


def legend_handles(extra=()):
    handles = [Line2D([], [], color=COLOURS[a], marker=MARKERS[a],
                      markeredgecolor="white", markeredgewidth=0.5,
                      label=a) for a in ALGORITHMS]
    return handles + list(extra)


def bars(axis, rows, groups, y, feasible_only=True, label_of=None):
    width = 0.8 / len(ALGORITHMS)
    for index, algorithm in enumerate(ALGORITHMS):
        stats = [mean_ci([y(r) for r in rows if r["label"] == g
                          and r["algorithm"] == algorithm
                          and (r["feasible"] or not feasible_only)])
                 for g in groups]
        positions = [g + (index - 1.5) * width for g in range(len(groups))]
        axis.bar(positions, [s[0] for s in stats], width=width,
                 yerr=[s[1] for s in stats], color=COLOURS[algorithm],
                 hatch=HATCHES[algorithm], edgecolor="white", linewidth=0.6,
                 capsize=1.2, error_kw={"ecolor": MUTED, "linewidth": 0.6},
                 label=algorithm)
    axis.set_xticks(range(len(groups)))
    axis.set_xticklabels([label_of(g) if label_of else g for g in groups])
    axis.grid(axis="x", visible=False)


def save(figure, folder, name):
    figure.savefig(os.path.join(folder, name + ".pdf"))
    plt.close(figure)


# ---- figures ------------------------------------------------------------------

def fig_sweep(results, out):
    rows = load(results, "sweep")
    figure, axes = plt.subplots(1, 3, figsize=(PAGE, 1.85))
    per_algorithm(axes[0], rows, "sigma", lambda r: r["cores_used"])
    xs, means, cis = series(rows, "sigma", lambda r: r["cores_lb"],
                            lambda r: r["algorithm"] == "BFDU")
    axes[0].plot(xs, means, color=INK, linestyle=":", linewidth=1.1)
    axes[0].set_ylabel("cores used $M$")
    axes[0].set_title("(a) cores, and the lower bound (dotted)")
    per_algorithm(axes[1], rows, "sigma", lambda r: r["hops_inter"])
    axes[1].set_ylabel("inter-ECU communications")
    axes[1].set_title("(b) inter-ECU communications")
    per_algorithm(axes[2], rows, "sigma", lambda r: r["sr_on_time"])
    axes[2].set_ylabel("task-chains on time (%)")
    axes[2].set_title("(c) task-chains meeting their deadline")
    for axis in axes:
        axis.set_xscale("log")
        ticks = [0.02, 0.05, 0.1, 0.2, 0.4, 0.8]
        axis.set_xticks(ticks)
        axis.set_xticklabels([f"{t:g}" for t in ticks])
        axis.xaxis.set_minor_locator(matplotlib.ticker.FixedLocator([0.03]))
        axis.xaxis.set_minor_formatter(matplotlib.ticker.NullFormatter())
        axis.set_xlabel(r"sub-chain utilization limit $\sigma$")
    lb = Line2D([], [], color=INK, linestyle=":", label="lower bound")
    figure.legend(handles=legend_handles([lb]), loc="lower center",
                  ncol=5, bbox_to_anchor=(0.5, 1.0))
    figure.tight_layout(w_pad=1.2)
    save(figure, out, "sweep")


def fig_tradeoff(results, out):
    """cores against chains on time, every instance of the default point"""
    rows = [r for r in load(results, "sweep") if abs(r["sigma"] - 0.4) < 1e-9
            and r["feasible"]]
    figure, axis = plt.subplots(figsize=(COLUMN, 2.0))
    for algorithm in ALGORITHMS:
        mine = [r for r in rows if r["algorithm"] == algorithm]
        axis.scatter([r["cores_used"] for r in mine],
                     [r["sr_on_time"] for r in mine], s=12,
                     color=COLOURS[algorithm], marker=MARKERS[algorithm],
                     edgecolors="white", linewidths=0.4, alpha=0.85,
                     label=algorithm)
        mx, _ = mean_ci([r["cores_used"] for r in mine])
        my, _ = mean_ci([r["sr_on_time"] for r in mine])
        axis.scatter([mx], [my], s=60, color=COLOURS[algorithm],
                     marker=MARKERS[algorithm], edgecolors=INK,
                     linewidths=0.8, zorder=5)
    axis.set_xlabel("cores used $M$")
    axis.set_ylabel("task-chains on time (%)")
    axis.legend(handles=legend_handles(), loc="lower right", ncol=2)
    save(figure, out, "tradeoff")


def fig_architecture(results, out):
    rows = load(results, "architecture")
    figure, axes = plt.subplots(1, 2, figsize=(COLUMN, 1.75))
    per_algorithm(axes[0], rows, "zcu_speed", lambda r: r["cores_used"])
    xs, means, _ = series(rows, "zcu_speed", lambda r: r["cores_lb"],
                          lambda r: r["algorithm"] == "BFDU")
    axes[0].plot(xs, means, color=INK, linestyle=":", linewidth=1.1)
    axes[0].set_ylabel("cores used $M$")
    per_algorithm(axes[1], rows, "zcu_speed", lambda r: r["sr_on_time"])
    axes[1].set_ylabel("task-chains on time (%)")
    for axis in axes:
        axis.set_xlabel("zone core speed (%)")
        axis.set_xticks([25, 50, 75, 100])
    figure.legend(handles=legend_handles(), loc="lower center", ncol=4,
                  bbox_to_anchor=(0.5, 1.0))
    figure.tight_layout(w_pad=1.0)
    save(figure, out, "architecture")


def fig_network(results, out):
    rows = load(results, "network")
    topo = load(results, "topology")
    figure, axes = plt.subplots(1, 3, figsize=(PAGE, 1.9),
                                gridspec_kw={"width_ratios": [1.35, 1.35, 1.3]})
    groups = ["100M default", "100M default preempt", "1000M default",
              "1000M default preempt", "100M sensors", "1000M sensors"]
    names = {"100M default": "100M", "100M default preempt": "100M\n+pre",
             "1000M default": "1G", "1000M default preempt": "1G\n+pre",
             "100M sensors": "100M\nlarge", "1000M sensors": "1G\nlarge"}
    bars(axes[0], rows, groups, lambda r: r["sr_on_time"],
         label_of=lambda g: names[g])
    axes[0].set_ylabel("task-chains on time (%)")
    axes[0].set_ylim(40, 100)
    axes[0].set_title("(a) link rate, preemption, message sizes")
    bars(axes[1], rows, groups, lambda r: r["port_max"],
         label_of=lambda g: names[g])
    axes[1].set_yscale("log")
    axes[1].axhline(75, color=INK, linestyle=":", linewidth=1.0)
    axes[1].set_ylabel("busiest port load (%)")
    axes[1].set_title("(b) port load (dotted: idle slope)")
    tg = ["star", "tree", "tree 10G", "tree ATS", "ring", "ring 10G",
          "ring ATS"]
    bars(axes[2], topo, tg, lambda r: r["net_max"] / 1000,
         label_of=lambda g: g.replace(" ", "\n"))
    axes[2].set_ylabel("largest network bound (ms)")
    axes[2].set_title("(c) topology and shaping")
    figure.legend(handles=legend_handles(), loc="lower center", ncol=4,
                  bbox_to_anchor=(0.5, 1.0))
    figure.tight_layout(w_pad=1.0)
    save(figure, out, "network")


def fig_constraints(results, out):
    cons = load(results, "constraints")
    cores = load(results, "cores")
    figure, axes = plt.subplots(1, 2, figsize=(PAGE, 1.95),
                                gridspec_kw={"width_ratios": [1.7, 1]})
    groups = ["none", "replicas 10%", "replicas 20%", "replicas 30%",
              "replicas 20% cores", "zone-only 20%", "co-located 30%"]
    names = {"none": "none", "replicas 10%": "repl.\n10%",
             "replicas 20%": "repl.\n20%", "replicas 30%": "repl.\n30%",
             "replicas 20% cores": "repl. 20%\ncores",
             "zone-only 20%": "zone-only\n20%",
             "co-located 30%": "co-loc.\n30%"}
    bars(axes[0], cons, groups, lambda r: r["cores_used"],
         label_of=lambda g: names[g])
    axes[0].set_ylabel("cores used $M$")
    axes[0].set_title("(a) spatial constraints")
    tg = ["8 big | 4 MCU", "4 big + 8 little | 4 MCU",
          "8 big | 2 MCU + 2 LS", "4 big + 8 little | 2 MCU + 2 LS"]
    tnames = {tg[0]: "8B | 4M", tg[1]: "4B+8L\n| 4M", tg[2]: "8B |\n2M+2LS",
              tg[3]: "4B+8L |\n2M+2LS"}
    bars(axes[1], cores, tg, lambda r: r["cores_used"],
         label_of=lambda g: tnames[g])
    axes[1].set_ylabel("cores used $M$")
    axes[1].set_title("(b) core types (B big, L little, M MCU, LS lock-step)")
    figure.legend(handles=legend_handles(), loc="lower center", ncol=4,
                  bbox_to_anchor=(0.5, 1.0))
    figure.tight_layout(w_pad=1.0)
    save(figure, out, "constraints")


def fig_deadlines(results, out):
    rows = load(results, "deadlines")
    figure, axis = plt.subplots(figsize=(COLUMN, 1.9))
    for algorithm in ALGORITHMS:
        for repair in (0, 1):
            xs, means, cis = series(
                rows, "deadline_pct", lambda r: r["sr_on_time"],
                lambda r, a=algorithm, k=repair: r["algorithm"] == a
                and r["repair"] == k)
            line(axis, xs, means, cis, algorithm, dashed=not repair,
                 label=None)
    axis.set_xlabel("end-to-end deadline (% of the pattern periods)")
    axis.set_xticks([200, 300, 400, 600])
    axis.set_ylabel("task-chains on time (%)")
    solid = Line2D([], [], color=MUTED, label="with local search")
    dashed = Line2D([], [], color=MUTED, linestyle="--",
                    label="without local search")
    axis.legend(handles=legend_handles([solid, dashed]), loc="lower right",
                ncol=2)
    save(figure, out, "deadlines")


def fig_margins(results, out):
    """cores against the WCET scaling factor guaranteed on the real WCETs"""
    rows = load(results, "margins")
    figure, axis = plt.subplots(figsize=(COLUMN, 1.95))
    for algorithm in ["BFDU", "CADU"]:
        margin = [r for r in rows if r["label"].startswith("margin")
                  and r["algorithm"] == algorithm and r["feasible"]]
        sized = [r for r in rows if r["label"].startswith("target")
                 and r["algorithm"] == algorithm and r["feasible"]]
        for subset, dashed, key in ((margin, True, "wcet_margin"),
                                    (sized, False, "target_scaling")):
            groups = defaultdict(list)
            for r in subset:
                groups[r[key]].append(r)
            xs, ys, cis = [], [], []
            for k in sorted(groups):
                g = groups[k]
                factor = [r["scaling"] * (1 + r["wcet_margin"] / 100)
                          for r in g]
                xs.append(mean_ci(factor)[0])
                m, c = mean_ci([r["cores_used"] for r in g])
                ys.append(m)
                cis.append(c)
            line(axis, xs, ys, cis, algorithm, dashed=dashed, label=None)
    axis.set_xlabel("critical scaling factor of the real WCETs")
    axis.set_ylabel("cores used $M$")
    solid = Line2D([], [], color=MUTED, label="sizing (target)")
    dashed = Line2D([], [], color=MUTED, linestyle="--",
                    label="WCET margin")
    handles = [h for h in legend_handles() if h.get_label() in
               ("BFDU", "CADU")]
    axis.legend(handles=handles + [solid, dashed], loc="upper left", ncol=2)
    save(figure, out, "margins")


def fig_scaling(results, out):
    rows = load(results, "scaling")
    figure, axis = plt.subplots(figsize=(COLUMN, 1.95))
    per_algorithm(axis, rows, "chains", lambda r: r["t_total"])
    xs = sorted({r["chains"] for r in rows})
    ref = [xs[0], xs[-1]]
    base = 1e-3
    axis.plot(ref, [base, base * (ref[1] / ref[0]) ** 2], color=MUTED,
              linestyle=":", linewidth=1.0)
    axis.text(ref[1] * 0.55, base * (ref[1] / ref[0]) ** 2 * 0.25,
              r"$\propto n^2$", color=MUTED, fontsize=7)
    axis.set_xscale("log")
    axis.set_yscale("log")
    axis.set_xticks(xs)
    axis.set_xticklabels([f"{int(x)}" for x in xs])
    axis.xaxis.set_minor_locator(matplotlib.ticker.NullLocator())
    axis.set_xlabel("task-chains $n$")
    axis.set_ylabel("CPU time of a design point (s)")
    axis.legend(handles=legend_handles(), loc="upper left", ncol=2)
    save(figure, out, "scaling")


def fig_exact(results, out):
    """gap of the heuristics to the optimum, with and without consolidation"""
    path = os.path.join(results, "exact.csv")
    if not os.path.exists(path):
        return
    with open(path, newline="") as handle:
        rows = [r for r in csv.DictReader(handle) if r["exact"]]
    configs = ["default", "slow zones", "constraints"]
    figure, axes = plt.subplots(1, 3, figsize=(PAGE, 1.85), sharey=True)
    for axis, config in zip(axes, configs):
        mine = [r for r in rows if r["config"] == config]
        sizes = sorted({int(r["chains"]) for r in mine})
        for algorithm in ["BFDU", "FFDU", "CADU"]:
            for suffix, dashed in (("", False), ("-", True)):
                means, cis = [], []
                for n in sizes:
                    gaps = [(float(r[algorithm + suffix]) - float(r["exact"]))
                            / float(r["exact"]) * 100 for r in mine
                            if int(r["chains"]) == n and r[algorithm + suffix]]
                    m, c = mean_ci(gaps)
                    means.append(m)
                    cis.append(c)
                line(axis, sizes, means, cis, algorithm, dashed=dashed,
                     label=None)
        axis.set_title(f"({'abc'[configs.index(config)]}) {config}")
        axis.set_xlabel("task-chains")
        axis.set_xticks(sizes)
    axes[0].set_ylabel("cores above the optimum (%)")
    solid = Line2D([], [], color=MUTED, label="with consolidation")
    dashed = Line2D([], [], color=MUTED, linestyle="--",
                    label="without consolidation")
    handles = [h for h in legend_handles() if h.get_label() != "WFDU"]
    figure.legend(handles=handles + [solid, dashed], loc="lower center",
                  ncol=5, bbox_to_anchor=(0.5, 1.0))
    figure.tight_layout(w_pad=1.0)
    save(figure, out, "exact")


def fig_frer(results, out):
    """frame replication on the ring: largest bound and backbone load"""
    rows = load(results, "frer")
    figure, axes = plt.subplots(1, 2, figsize=(COLUMN, 1.8))
    styles = {("CBS", 0): ("#2a78d6", "o", "-", "CBS"),
              ("CBS", 1): ("#2a78d6", "o", "--", "CBS + FRER"),
              ("ATS", 0): ("#eb6834", "s", "-", "ATS"),
              ("ATS", 1): ("#eb6834", "s", "--", "ATS + FRER")}
    for (shaping, frer), (colour, marker, ls, label) in styles.items():
        for axis, key, scale in ((axes[0], "net_max", 1e-3),
                                 (axes[1], "backbone_max", 1)):
            xs, means, cis = series(
                rows, "replica_pct", lambda r, k=key, s=scale: r[k] * s,
                lambda r, sh=shaping, f=frer: r["algorithm"] == "BFDU" and
                r["shaping"] == sh and (r["frer"] == f or r["replica_pct"] == 0))
            axis.plot(xs, means, color=colour, marker=marker, linestyle=ls,
                      label=label, markeredgecolor="white",
                      markeredgewidth=0.5)
            axis.fill_between(xs, [m - c for m, c in zip(means, cis)],
                              [m + c for m, c in zip(means, cis)],
                              color=colour, alpha=0.12, linewidth=0)
    axes[0].set_ylabel("largest network bound (ms)")
    axes[1].set_ylabel("busiest backbone link (%)")
    for axis in axes:
        axis.set_xlabel("replicated task-chains (%)")
        axis.set_xticks([0, 10, 20, 30])
    handles, labels = axes[0].get_legend_handles_labels()
    figure.legend(handles, labels, loc="lower center", ncol=4,
                  bbox_to_anchor=(0.5, 1.0))
    figure.tight_layout(w_pad=1.0)
    save(figure, out, "frer")


def fig_contention(results, out):
    rows = load(results, "contention")
    figure, axes = plt.subplots(1, 2, figsize=(COLUMN, 1.75))
    per_algorithm(axes[0], rows, "memory_latency", lambda r: r["cores_used"])
    axes[0].set_ylabel("cores used $M$")
    per_algorithm(axes[1], rows, "memory_latency", lambda r: r["u_app"])
    axes[1].set_ylabel("application load (cores)")
    for axis in axes:
        axis.set_xlabel("memory latency (ns)")
        axis.set_xticks([0, 20, 50, 100])
    figure.legend(handles=legend_handles(), loc="lower center", ncol=4,
                  bbox_to_anchor=(0.5, 1.0))
    figure.tight_layout(w_pad=1.0)
    save(figure, out, "contention")


def fig_lifecycle(results, out):
    """platform cost against updates absorbed; margin along the life"""
    rows = load(results, "lifecycle")
    figure, axes = plt.subplots(1, 2, figsize=(PAGE * 0.62, 1.9))
    for algorithm in ALGORITHMS:
        mine = [r for r in rows if r["algorithm"] == algorithm and
                r["feasible"]]
        groups = defaultdict(list)
        for r in mine:
            groups[r["phi"]].append(r)
        xs, ys, xe, ye = [], [], [], []
        for phi in sorted(groups):
            g = groups[phi]
            mx, cx = mean_ci([r["cores_used"] for r in g])
            my, cy = mean_ci([r["absorbed"] for r in g])
            xs.append(mx)
            ys.append(my)
            xe.append(cx)
            ye.append(cy)
        axes[0].errorbar(xs, ys, xerr=xe, yerr=ye, color=COLOURS[algorithm],
                         marker=MARKERS[algorithm], markeredgecolor="white",
                         markeredgewidth=0.5, capsize=1.2, elinewidth=0.6,
                         label=algorithm)
        for x, y, phi in zip(xs, ys, sorted(groups)):
            if algorithm == "BFDU":
                axes[0].annotate(f"{phi:.1f}", (x, y), textcoords="offset "
                                 "points", xytext=(3, -8), fontsize=6,
                                 color=MUTED)
    axes[0].set_xlabel("cores of the platform (SOP)")
    axes[0].set_ylabel("updates absorbed (of 10)")
    axes[0].set_title(r"(a) cost and life ($\varphi_{SOP}$ labelled)")
    for phi, ls in ((0.5, "-"), (0.6, "--"), (0.7, ":")):
        means = []
        for u in range(11):
            values = [r[f"life_scaling_{u}"] for r in rows
                      if r["algorithm"] == "BFDU" and abs(r["phi"] - phi) < 1e-6
                      and isinstance(r.get(f"life_scaling_{u}"), float)]
            means.append(mean_ci(values)[0] if values else math.nan)
        axes[1].plot(range(11), means, color=COLOURS["BFDU"], linestyle=ls,
                     label=rf"$\varphi_{{SOP}}={phi}$")
    axes[1].set_xlabel("software update")
    axes[1].set_ylabel("critical scaling factor")
    axes[1].set_title("(b) margin along the life (BFDU)")
    axes[1].legend(loc="upper right")
    axes[0].legend(handles=legend_handles(), loc="upper left", ncol=1)
    figure.tight_layout(w_pad=1.2)
    save(figure, out, "lifecycle")


SEARCH_COLOURS = {"SA": "#8a5cc8", "GA": "#c43d7a"}
SEARCH_MARKERS = {"SA": "v", "GA": "P"}


def fig_baselines(results, out):
    """cores above the optimum against CPU time: the heuristics, simulated
    annealing and the genetic algorithm at three budgets, the exact
    formulation"""
    base_path = os.path.join(results, "baselines.csv")
    exact_path = os.path.join(results, "exact.csv")
    if not (os.path.exists(base_path) and os.path.exists(exact_path)):
        return
    with open(exact_path, newline="") as handle:
        exact = {(r["chains"], r["seed"]): r for r in csv.DictReader(handle)
                 if r["config"] == "default" and r["status"] == "OPTIMAL"}
    with open(base_path, newline="") as handle:
        base = [r for r in csv.DictReader(handle)
                if (r["chains"], r["seed"]) in exact and r["feasible"] == "1"]
    sizes = sorted({int(r["chains"]) for r in base})
    figure, axes = plt.subplots(1, len(sizes), figsize=(PAGE, 1.9),
                                sharey=True)

    def gap(r):
        e = float(exact[(r["chains"], r["seed"])]["exact"])
        return (float(r["cores"]) - e) / e * 100

    for axis, n in zip(axes, sizes):
        mine = [r for r in base if int(r["chains"]) == n]
        for method in ("SA", "GA"):
            budgets = sorted({int(r["budget"]) for r in mine
                              if r["method"] == method})
            xs, ys, cis = [], [], []
            for b in budgets:
                rows = [r for r in mine if r["method"] == method
                        and int(r["budget"]) == b]
                xs.append(sum(float(r["cpu_s"]) for r in rows) / len(rows))
                m, c = mean_ci([gap(r) for r in rows])
                ys.append(m)
                cis.append(c)
            axis.errorbar(xs, ys, yerr=cis, color=SEARCH_COLOURS[method],
                          marker=SEARCH_MARKERS[method], capsize=1.5,
                          markeredgecolor="white", markeredgewidth=0.5,
                          elinewidth=0.6, label=method)
        for algorithm in ("BFDU", "FFDU", "CADU"):
            rows = [r for r in mine if r["method"] == algorithm]
            t = sum(float(r["cpu_s"]) for r in rows) / len(rows)
            m, c = mean_ci([gap(r) for r in rows])
            axis.errorbar([t], [m], yerr=[c], color=COLOURS[algorithm],
                          marker=MARKERS[algorithm], capsize=1.5,
                          markeredgecolor="white", markeredgewidth=0.5,
                          elinewidth=0.6, linestyle="none")
        times = [float(r["time"]) for k, r in exact.items()
                 if int(k[0]) == n]
        axis.plot([sum(times) / len(times)], [0], color=INK, marker="*",
                  markersize=8, linestyle="none", markeredgecolor="white",
                  markeredgewidth=0.4, zorder=6)
        axis.set_xscale("log")
        axis.set_xlim(5e-4, 200)
        axis.set_title(f"({'abcd'[sizes.index(n)]}) {n} task-chains")
        axis.set_xlabel("time (s, log)")
    axes[0].set_ylabel("cores above the optimum (%)")
    handles = [h for h in legend_handles() if h.get_label() != "WFDU"]
    handles += [Line2D([], [], color=SEARCH_COLOURS[m],
                       marker=SEARCH_MARKERS[m], markeredgecolor="white",
                       markeredgewidth=0.5,
                       label={"SA": "simulated annealing",
                              "GA": "genetic algorithm"}[m])
                for m in ("SA", "GA")]
    handles.append(Line2D([], [], color=INK, marker="*", linestyle="none",
                          markersize=7, label="exact (optimum)"))
    figure.legend(handles=handles, loc="lower center", ncol=6,
                  bbox_to_anchor=(0.5, 1.0))
    figure.tight_layout(w_pad=0.8)
    save(figure, out, "baselines")


DSE_ARCHITECTURES = ["distributed", "domain", "hybrid", "zonal"]


def fig_dse(results, out):
    """the same applications on four E/E architectures"""
    if not os.path.exists(os.path.join(results, "dse.csv")):
        return
    rows = load(results, "dse")
    for r in rows:
        r["architecture"] = r["label"].split()[0]
    figure, axes = plt.subplots(2, 4, figsize=(PAGE, 3.3), sharex=True,
                                sharey="row")
    for col, arch in enumerate(DSE_ARCHITECTURES):
        mine = [r for r in rows if r["architecture"] == arch]
        for algorithm in ("FFDU", "CADU"):
            for row_index, key in enumerate(("cores_used", "hops_inter")):
                xs, means, cis = series(
                    mine, "chains", lambda r, k=key: r[k],
                    lambda r, a=algorithm: r["algorithm"] == a)
                line(axes[row_index][col], xs, means, cis, algorithm)
            # the share of feasible instances, where some are not
            shares = []
            for x in sorted({r["chains"] for r in mine}):
                point = [r for r in mine if r["chains"] == x
                         and r["algorithm"] == algorithm]
                shares.append(100 * sum(r["feasible"] for r in point)
                              / len(point))
            if min(shares) < 100:
                axes[0][col].text(
                    0.97, 0.06 if algorithm == "CADU" else 0.17,
                    f"{algorithm} feasible: " +
                    " / ".join(f"{v:.0f}" for v in shares) + " %",
                    transform=axes[0][col].transAxes, ha="right",
                    fontsize=6, color=COLOURS[algorithm])
        xs, means, _ = series(mine, "chains", lambda r: r["cores_lb"],
                              lambda r: r["algorithm"] == "FFDU")
        axes[0][col].plot(xs, means, color=MUTED, linestyle=":",
                          linewidth=1.0)
        axes[0][col].set_title(f"({'abcd'[col]}) {arch}")
        axes[1][col].set_xlabel("task-chains")
        axes[1][col].set_xticks([100, 200, 300])
    axes[0][0].set_ylabel("cores used")
    axes[1][0].set_ylabel("inter-ECU communications")
    handles = [h for h in legend_handles()
               if h.get_label() in ("FFDU", "CADU")]
    handles.append(Line2D([], [], color=MUTED, linestyle=":",
                          label="lower bound"))
    figure.legend(handles=handles, loc="lower center", ncol=3,
                  bbox_to_anchor=(0.5, 1.0))
    figure.tight_layout(h_pad=0.6, w_pad=0.8)
    save(figure, out, "dse")


SYSSIM_SETTINGS = [("zonal-star", "zonal, star, FFDU"),
                   ("zonal-star-cadu", "zonal, star, CADU"),
                   ("zonal-ring-frer", "zonal, ring, FRER"),
                   ("zonal-tree-tas", "zonal, tree, TAS"),
                   ("zonal-contention", "zonal, contention"),
                   ("zonal-star-let", "zonal, star, strict LET"),
                   ("zonal-ring-let", "zonal, ring, strict LET"),
                   ("domain", "domain"),
                   ("hybrid", "hybrid")]


def fig_syssim(results, out):
    """observed over bound in the full-system simulation"""
    path = os.path.join(results, "syssim.csv")
    if not os.path.exists(path):
        return
    with open(path, newline="") as handle:
        rows = list(csv.DictReader(handle))
    figure, axes = plt.subplots(1, 2, figsize=(COLUMN, 2.6), sharey=True)
    for axis, (obs, bound, title) in zip(axes, (
            ("end_to_end_observed_us", "end_to_end_bound_us",
             "(a) end-to-end delay"),
            ("reaction_observed_us", "reaction_bound_us",
             "(b) maximum reaction time"))):
        data = [[float(r[obs]) / float(r[bound]) for r in rows
                 if r["setting"] == s] for s, _ in SYSSIM_SETTINGS]
        parts = axis.boxplot(data, orientation="horizontal", whis=(5, 95), widths=0.55,
                             showfliers=False, patch_artist=True,
                             medianprops={"color": INK, "linewidth": 0.9},
                             whiskerprops={"color": MUTED, "linewidth": 0.6},
                             capprops={"color": MUTED, "linewidth": 0.6})
        for box in parts["boxes"]:
            box.set(facecolor=COLOURS["BFDU"] + "40",
                    edgecolor=COLOURS["BFDU"], linewidth=0.6)
        axis.plot([max(d) for d in data], range(1, len(data) + 1),
                  linestyle="none", marker="|", color=INK, markersize=5)
        axis.axvline(1.0, color=COLOURS["WFDU"], linewidth=0.9)
        axis.set_xlim(0.4, 1.04)
        axis.set_title(title)
        axis.set_xlabel("observed / bound")
        axis.grid(axis="y", visible=False)
    axes[0].set_yticks(range(1, len(SYSSIM_SETTINGS) + 1))
    axes[0].set_yticklabels([name for _, name in SYSSIM_SETTINGS])
    axes[0].invert_yaxis()
    figure.tight_layout(w_pad=0.6)
    save(figure, out, "syssim")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--results", default="../results")
    parser.add_argument("--out", default="figures")
    args = parser.parse_args()
    os.makedirs(args.out, exist_ok=True)
    style()
    for make in (fig_sweep, fig_tradeoff, fig_architecture, fig_network,
                 fig_constraints, fig_deadlines, fig_margins, fig_scaling,
                 fig_exact, fig_frer, fig_contention, fig_lifecycle,
                 fig_baselines, fig_dse, fig_syssim):
        make(args.results, args.out)
        print(make.__name__)


if __name__ == "__main__":
    main()
