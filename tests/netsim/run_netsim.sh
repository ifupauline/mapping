#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
# Network Calculus bounds vs frame-level simulation: export flow sets from
# real mappings and random heavy loads, on the three topologies, with one
# or three CBS classes, with time-aware shaping and with asynchronous
# traffic shaping, with frame replication (FRER) on the ring and on a
# custom topology, and on the domain and distributed templates; simulate
# them; fail on any delay above its bound.
# Usage: run_netsim.sh <net_export executable> <netsim.py> [horizon us] [runs]
# (default 10000 us and 1 run; the paper uses 20000 and 2)
set -e
dir=$(mktemp -d)
trap 'rm -rf "$dir"' EXIT
"$1" heavy 50 1 "$dir/heavy.csv"
"$1" heavy 70 2 "$dir/heavy70.csv"
"$1" heavy 60 3 "$dir/tree.csv" --topology tree
"$1" heavy 60 4 "$dir/ring.csv" --topology ring
"$1" heavy 60 5 "$dir/classes.csv" --classes 3
"$1" heavy 50 6 "$dir/tas.csv" --tas --classes 3
"$1" heavy 60 7 "$dir/ats.csv" --ats --classes 3
"$1" heavy 60 8 "$dir/ats_ring.csv" --ats --topology ring
"$1" heavy 40 9 "$dir/frer_ring.csv" --topology ring --frer
"$1" heavy 40 10 "$dir/frer_ats.csv" --topology ring --frer --ats --classes 3
custom=$(dirname "$2")/custom.json
"$1" heavy 40 11 "$dir/custom.csv" --config "$custom"
"$1" heavy 40 12 "$dir/custom_ats.csv" --config "$custom" --ats --classes 3
"$1" map 0.3 1 "$dir/map_custom.csv" --config "$custom" --classes 3
templates=$(dirname "$2")/../../examples/templates
"$1" map 0.3 1 "$dir/map_domain.csv" --config "$templates/domain.json" --topology tree
"$1" map 0.3 1 "$dir/map_distributed.csv" --config "$templates/distributed.json" --ats
for algorithm in BFDU WFDU FFDU CADU; do
	"$1" map 0.3 1 "$dir/map_$algorithm.csv" --algorithm $algorithm
done
"$1" map 0.3 1 "$dir/map_ring.csv" --topology ring --classes 3
"$1" map 0.3 1 "$dir/map_tree_tas.csv" --topology tree --tas
"$1" map 0.3 1 "$dir/map_tree_ats.csv" --topology tree --ats --classes 3
"$1" map 0.3 1 "$dir/map_frer.csv" --topology ring --frer --classes 3
for f in "$dir"/*.csv; do
	python3 "$2" "$f" "${3:-10000}" "${4:-1}" | tee "$dir/out.txt"
	grep -q "violations = 0" "$dir/out.txt"
done
