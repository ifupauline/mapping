#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Figures of the experiments.

Reads the CSV files written by `build/bin/experiment <name>` and draws one
multi-panel figure per experiment: means over the instances with their
95 % confidence interval, one colour and one marker per bin-packing
algorithm (colour-blind safe palette), a legend on every figure.

usage:
    python3 plots.py <experiment|all> [--results DIR] [--save DIR] [--pdf]

    --results DIR  folder of the CSV files (default: results)
    --save DIR     write the figures to DIR (PNG, or PDF with --pdf)
                   instead of opening them in a window
    --pdf          vector output, for LaTeX
"""
import argparse
import csv
import math
import os
import sys
from collections import defaultdict

import matplotlib
import matplotlib.ticker

# series colours (fixed order, validated for colour-vision deficiencies)
# and markers, so that a series never depends on colour alone
COLOURS = ["#2a78d6", "#eb6834", "#1baf7a", "#eda100"]
MARKERS = ["o", "s", "^", "D"]
HATCHES = ["", "//", "..", "xx"]
INK, MUTED, GRID = "#0b0b0b", "#52514e", "#e4e3df"
ALGORITHMS = ["BFDU", "WFDU", "FFDU", "CADU"]


def style():
    """Thin marks, recessive grid and axes, readable sizes."""
    matplotlib.rcParams.update({
        "font.size": 9, "axes.titlesize": 9, "axes.labelsize": 9,
        "legend.fontsize": 8, "xtick.labelsize": 8, "ytick.labelsize": 8,
        "axes.edgecolor": MUTED, "axes.labelcolor": INK,
        "xtick.color": MUTED, "ytick.color": MUTED,
        "axes.grid": True, "grid.color": GRID, "grid.linewidth": 0.6,
        "axes.spines.top": False, "axes.spines.right": False,
        "lines.linewidth": 1.5, "lines.markersize": 5,
        "legend.frameon": False, "figure.dpi": 110, "savefig.dpi": 200,
        "pdf.fonttype": 42,
    })


def load(results, name):
    """Rows of results/<name>.csv as dictionaries of numbers or strings."""
    path = os.path.join(results, name + ".csv")
    if not os.path.exists(path):
        sys.exit(f"{path} not found: run build/bin/experiment {name} first")
    rows = []
    with open(path, newline="") as handle:
        for row in csv.DictReader(handle):
            for key, value in row.items():
                if key in ("label", "algorithm", "application", "experiment"):
                    continue
                try:
                    row[key] = float(value)
                except ValueError:
                    pass
            if row["error"] == 0:
                rows.append(row)
    return rows


def mean_ci(values):
    """Mean and half-width of its 95 % confidence interval."""
    values = [v for v in values if not math.isnan(v)]
    if not values:
        return math.nan, 0.0
    mean = sum(values) / len(values)
    if len(values) < 2:
        return mean, 0.0
    variance = sum((v - mean) ** 2 for v in values) / (len(values) - 1)
    return mean, 1.96 * math.sqrt(variance / len(values))


def series(rows, x_key, y, select, feasible_only=True):
    """Points (x, mean, ci) of y(row) for the rows chosen by select."""
    groups = defaultdict(list)
    for row in rows:
        if select(row) and (row["feasible"] or not feasible_only):
            groups[row[x_key]].append(y(row))
    xs = sorted(groups)
    stats = [mean_ci(groups[x]) for x in xs]
    return xs, [s[0] for s in stats], [s[1] for s in stats]


def draw(axis, xs, means, cis, index, label):
    """One series: line, markers and its confidence band."""
    colour = COLOURS[index % len(COLOURS)]
    axis.plot(xs, means, color=colour, marker=MARKERS[index % len(MARKERS)],
              label=label)
    low = [m - c for m, c in zip(means, cis)]
    high = [m + c for m, c in zip(means, cis)]
    axis.fill_between(xs, low, high, color=colour, alpha=0.15, linewidth=0)


def panels(count, title, width=7.0):
    import matplotlib.pyplot as plt
    columns = 2 if count > 1 else 1
    rows = (count + 1) // 2
    figure, axes = plt.subplots(rows, columns,
                                figsize=(width, 2.5 * rows), squeeze=False)
    figure.suptitle(title, fontsize=10, color=INK, x=0.01, ha="left")
    flat = [a for line in axes for a in line]
    for extra in flat[count:]:
        extra.set_visible(False)
    return figure, flat[:count]


def per_algorithm(axes, rows, x_key, metrics, x_label, log_x=False):
    """One panel per metric, one series per bin-packing algorithm."""
    for axis, (label, y, feasible_only) in zip(axes, metrics):
        for index, algorithm in enumerate(ALGORITHMS):
            xs, means, cis = series(
                rows, x_key, y, lambda r, a=algorithm: r["algorithm"] == a,
                feasible_only)
            if xs:
                draw(axis, xs, means, cis, index, algorithm)
        axis.set_xlabel(x_label)
        axis.set_ylabel(label)
        if log_x:
            ticks = sorted({row[x_key] for row in rows})
            axis.set_xscale("log")
            axis.set_xticks(ticks)
            axis.set_xticklabels([f"{t:g}" for t in ticks])
            axis.xaxis.set_minor_locator(matplotlib.ticker.NullLocator())
    axes[0].legend()


def grouped_bars(axes, rows, metrics, groups, feasible_only=None):
    """Bars per configuration label, one bar per algorithm in a group."""
    width = 0.8 / len(ALGORITHMS)
    if feasible_only is None:
        feasible_only = [True] * len(metrics)
    for axis, (label, y), only in zip(axes, metrics, feasible_only):
        for index, algorithm in enumerate(ALGORITHMS):
            stats = [mean_ci([y(r) for r in rows
                              if r["label"] == g
                              and (r["feasible"] or not only)
                              and r["algorithm"] == algorithm])
                     for g in groups]
            positions = [g + (index - (len(ALGORITHMS) - 1) / 2) * width
                         for g in range(len(groups))]
            axis.bar(positions, [s[0] for s in stats], width=width,
                     yerr=[s[1] for s in stats],
                     color=COLOURS[index], hatch=HATCHES[index],
                     edgecolor="white", linewidth=1, capsize=2,
                     error_kw={"ecolor": MUTED, "linewidth": 1},
                     label=algorithm)
        axis.set_xticks(list(range(len(groups))))
        axis.set_xticklabels(groups, rotation=35, ha="right",
                             rotation_mode="anchor")
        axis.set_ylabel(label)
        axis.grid(axis="x", visible=False)
    # one legend for the figure, above the panels
    handles, names = axes[0].get_legend_handles_labels()
    axes[0].figure.legend(handles, names, loc="upper right", ncol=4,
                          bbox_to_anchor=(0.98, 0.99))


def labels_in_order(rows):
    seen = []
    for row in rows:
        if row["label"] not in seen:
            seen.append(row["label"])
    return seen


# common metrics: (label, value, feasible rows only)
FEASIBLE = ("feasible applications (%)", lambda r: 100 * r["feasible"], False)
CORES = ("cores used M", lambda r: r["cores_used"], True)
ON_TIME = ("chains on time (%)", lambda r: r["sr_on_time"], True)
SR = ("schedulable cores SR (%)", lambda r: r["sr_final"], True)
SR_SCHED = ("SR after scheduling (%)", lambda r: r["sr_sched"], True)
SCALING = ("WCET scaling factor", lambda r: r["scaling"], True)
AGE = ("max end-to-end delay (ms)", lambda r: r["delay_max"] / 1e3, True)


def plot_sweep(rows):
    figure, axes = panels(6, "sweep: partitioning limit sigma x bin packing")
    per_algorithm(axes, rows, "sigma", [
        CORES,
        ("cores on the ZCUs", lambda r: r["cores_zcu"], True),
        ("fragmentation rate FR (%)", lambda r: r["fr"], True),
        ("network hops per application", lambda r: r["hops_inter"], True),
        ON_TIME, AGE,
    ], "sigma (fraction of a core)", log_x=True)
    return figure


def fit_exponent(xs, means):
    """k of means ~ xs^k (least squares on the logarithms), or None."""
    points = [(x, m) for x, m in zip(xs, means) if m > 0]
    if len(points) < 2:
        return None
    lx = [math.log(x) for x, _ in points]
    ly = [math.log(m) for _, m in points]
    n, sx, sy = len(lx), sum(lx), sum(ly)
    return (n * sum(a * b for a, b in zip(lx, ly)) - sx * sy) / \
        (n * sum(a * a for a in lx) - sx * sx)


def plot_scaling(rows):
    figure, axes = panels(4, "scaling: CPU time and cores against the "
                          "number of task-chains")
    metrics = [("CPU time of a mapping (s)", lambda r: r["t_total"]),
               ("CPU time of placement and repair (s)",
                lambda r: r["t_swap"] + r["t_disp"] + r["t_repair"]),
               ("CPU time of scheduling (s)", lambda r: r["t_sched"]),
               ("cores used M", lambda r: r["cores_used"])]
    for panel, (axis, (label, y)) in enumerate(zip(axes, metrics)):
        for index, algorithm in enumerate(ALGORITHMS):
            xs, means, cis = series(
                rows, "chains", y, lambda r, a=algorithm: r["algorithm"] == a,
                panel == 3)
            name = algorithm
            k = fit_exponent(xs, means) if panel < 3 else None
            if k is not None:
                name += f" (n^{k:.2f})"
            # two instances: no meaningful interval on a log scale
            draw(axis, xs, means, [0] * len(xs), index, name)
        axis.set_xscale("log", base=2)
        axis.set_xticks(xs)
        axis.set_xticklabels([f"{x:g}" for x in xs])
        axis.xaxis.set_minor_locator(matplotlib.ticker.NullLocator())
        if panel < 3:
            axis.set_yscale("log")
        axis.set_xlabel("task-chains n")
        axis.set_ylabel(label)
        axis.legend()
    return figure


def plot_architecture(rows):
    figure, axes = panels(4, "architecture: speed of the zone controllers "
                          "(PCU at 100 %)")
    per_algorithm(axes, rows, "zcu_speed", [
        CORES,
        ("cores on the ZCUs", lambda r: r["cores_zcu"], True),
        ("application load (cores)", lambda r: r["u_app"], True),
        ON_TIME,
    ], "ZCU core speed (% of the PCU)")
    return figure


def plot_affinity(rows):
    figure, axes = panels(4, "affinity: fragments of a chain kept together")
    grouped_bars(axes, rows, [
        ("cores used M", lambda r: r["cores_used"]),
        ("hops between cores of one ECU", lambda r: r["hops_intra"]),
        ("network hops", lambda r: r["hops_inter"]),
        ("chains on time (%)", lambda r: r["sr_on_time"]),
    ], labels_in_order(rows))
    return figure


def plot_network(rows):
    figure, axes = panels(4, "network: link rate x frame preemption x "
                          "message sizes")
    grouped_bars(axes, rows, [
        ("max network delay bound (us)", lambda r: r["net_max"]),
        ("busiest port load (%)", lambda r: r["port_max"]),
        ("chains on time (%)", lambda r: r["sr_on_time"]),
        ("max end-to-end delay (ms)", lambda r: r["delay_max"] / 1e3),
    ], labels_in_order(rows))
    axes[0].set_yscale("log")
    axes[1].set_yscale("log")
    return figure


def plot_topology(rows):
    figure, axes = panels(4, "topology: star, two-level tree, ring "
                          "(backbone at 1 or 10 Gbit/s)")
    grouped_bars(axes, rows, [
        ("max network delay bound (us)", lambda r: r["net_max"]),
        ("busiest port load (%)", lambda r: r["port_max"]),
        ("chains on time (%)", lambda r: r["sr_on_time"]),
        ("max end-to-end delay (ms)", lambda r: r["delay_max"] / 1e3),
    ], labels_in_order(rows))
    return figure


def plot_classes(rows):
    figure, axes = panels(4, "classes: LET traffic classes and shaping, "
                          "whole vehicle")
    grouped_bars(axes, rows, [
        ("max network delay bound (us)", lambda r: r["net_max"]),
        ("mean network delay bound (us)", lambda r: r["net_avg"]),
        ("chains on time (%)", lambda r: r["sr_on_time"]),
        ("deadline misses (chains)", lambda r: r["deadline_misses"]),
    ], labels_in_order(rows))
    axes[0].set_yscale("log")
    return figure


def plot_constraints(rows):
    figure, axes = panels(4, "constraints: replicas, zone-only chains, "
                          "co-location, memory")
    grouped_bars(axes, rows, [
        ("feasible applications (%)", lambda r: 100 * r["feasible"]),
        ("cores on the PCU", lambda r: r["cores_pcu"]),
        ("cores on the ZCUs", lambda r: r["cores_zcu"]),
        ("chains on time (%)", lambda r: r["sr_on_time"]),
    ], labels_in_order(rows), feasible_only=[False, True, True, True])
    return figure


def plot_cores(rows):
    figure, axes = panels(4, "cores: types of cores (PCU | zone controllers)")
    grouped_bars(axes, rows, [
        ("cores on the PCU", lambda r: r["cores_pcu"]),
        ("cores on the ZCUs", lambda r: r["cores_zcu"]),
        ("application load (cores)", lambda r: r["u_app"]),
        ("chains on time (%)", lambda r: r["sr_on_time"]),
    ], labels_in_order(rows))
    return figure


def plot_fill(rows):
    figure, axes = panels(4, "fill: filling limit phi of the cores")
    per_algorithm(axes, rows, "phi", [CORES, SR_SCHED, SCALING, ON_TIME],
                  "phi")
    return figure


def plot_let(rows):
    figure, axes = panels(4, "LET: WCET of the LET task")
    per_algorithm(axes, rows, "let_wcet", [
        ("LET load (cores)", lambda r: r["u_let"], True),
        CORES, ON_TIME, AGE,
    ], "LET WCET on the reference core (us)")
    return figure


def plot_margins(rows):
    """Two ways to a margin: WCETs increased, or phi sized for a target."""
    figure, axes = panels(4, "margins: WCET margin at design time, or "
                          "sizing for a WCET scaling factor")
    margin = [r for r in rows if r["label"].startswith("margin")]
    sized = [r for r in rows if r["label"].startswith("target")]
    per_algorithm(axes[:2], margin, "wcet_margin", [
        CORES,
        # the factor of the mapping applies to the increased WCETs
        ("scaling factor of the real WCETs",
         lambda r: r["scaling"] * (1 + r["wcet_margin"] / 100), True),
    ], "WCET margin (%)")
    per_algorithm(axes[2:], sized, "target_scaling", [
        CORES,
        ("phi found by the sizing", lambda r: r["sized_phi"], True),
    ], "target WCET scaling factor")
    return figure


def plot_deadlines(rows):
    figure, axes = panels(4, "deadlines: end-to-end deadline x deadline "
                          "repair")
    grouped_bars(axes, rows, [
        ("chains on time (%)", lambda r: r["sr_on_time"]),
        ("deadline misses (chains)", lambda r: r["deadline_misses"]),
        ("repair moves", lambda r: r["repairs"]),
        ("cores used M", lambda r: r["cores_used"]),
    ], labels_in_order(rows))
    return figure


def plot_detection(rows):
    figure, axes = panels(2, "detection chain: cores used "
                          "against sigma")
    for axis, phi in zip(axes, [1.0, 0.8]):
        selected = [r for r in rows if abs(r["phi"] - phi) < 1e-9]
        per_algorithm([axis], selected, "sigma",
                      [("cores used M", lambda r: r["cores_used"], True)],
                      "sigma")
        axis.set_title(f"phi = {phi:g}")
        axis.yaxis.get_major_locator().set_params(integer=True)
    return figure


def plot_vehicle(rows):
    figure, axes = panels(4, "vehicle: the software of a whole vehicle")
    grouped_bars(axes, rows, [
        ("cores on the PCU", lambda r: r["cores_pcu"]),
        ("cores on the ZCUs", lambda r: r["cores_zcu"]),
        ("chains on time (%)", lambda r: r["sr_on_time"]),
        ("busiest port load (%)", lambda r: r["port_max"]),
    ], labels_in_order(rows))
    return figure


PLOTS = {"sweep": plot_sweep, "scaling": plot_scaling,
         "architecture": plot_architecture, "affinity": plot_affinity,
         "network": plot_network, "topology": plot_topology,
         "classes": plot_classes, "constraints": plot_constraints,
         "cores": plot_cores, "fill": plot_fill, "let": plot_let,
         "margins": plot_margins, "deadlines": plot_deadlines,
         "detection": plot_detection, "vehicle": plot_vehicle}


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("experiment", choices=sorted(PLOTS) + ["all"])
    parser.add_argument("--results", default="results")
    parser.add_argument("--save", metavar="DIR")
    parser.add_argument("--pdf", action="store_true")
    options = parser.parse_args()
    if options.save:
        matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    style()

    names = sorted(PLOTS) if options.experiment == "all" else \
        [options.experiment]
    for name in names:
        if options.experiment == "all" and not os.path.exists(
                os.path.join(options.results, name + ".csv")):
            print(f"  {name}: no results, skipped")
            continue
        figure = PLOTS[name](load(options.results, name))
        figure.tight_layout(rect=(0, 0, 1, 0.97))
        if not options.save:
            continue
        os.makedirs(options.save, exist_ok=True)
        path = os.path.join(options.save,
                            name + (".pdf" if options.pdf else ".png"))
        figure.savefig(path)
        print(f"  {name}: {path}")
        plt.close(figure)
    if not options.save:
        plt.show()


if __name__ == "__main__":
    main()
