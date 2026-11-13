#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Rows of the summary table of mapping, as plain columns.
Usage: table_rows.py LABEL ROW [ROW...] < output of mapping
prints: LABEL value(s) for every ROW (a prefix of the metric name)."""
import sys

label, wanted = sys.argv[1], sys.argv[2:]
values = {}
for line in sys.stdin:
    cells = [c.strip() for c in line.split("\u2502")]
    if len(cells) < 3:
        continue
    for w in wanted:
        if cells[1].startswith(w) and w not in values:
            values[w] = [c for c in cells[2:] if c]
out = []
for w in wanted:
    out += values.get(w, ["-"])
print(f"{label:<22}" + "".join(f"{v:>14}" for v in out))
