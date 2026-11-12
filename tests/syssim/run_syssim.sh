#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
# Bounds of the analysis vs full-system simulation: map applications on
# the zonal platform (star; ring with FRER, three classes and memory
# contention; tree with TAS; ATS), on the domain and distributed templates
# and on a custom topology, export their timing model and simulate it;
# fail on any end-to-end delay or reaction time above its bound.
# Usage: run_syssim.sh <mapping executable> <syssim.py> [scenarios]
set -e
mapping=$1
sim=$2
dir=$(mktemp -d)
trap 'rm -rf "$dir"' EXIT
root=$(dirname "$sim")/../..
n=${3:-2}
run() {
	name=$1
	shift
	"$mapping" "$@" --out "$dir/$name" > /dev/null
	python3 "$sim" "$dir/$name/system.json" "$n" | tee "$dir/out.txt"
	! grep -q "violations = [1-9]" "$dir/out.txt"
}
run star --chains 40 --seed 1
run ring --chains 40 --seed 2 --topology ring --frer --replicas 20 \
	--classes 3 --memory-latency 20 --algorithm CADU
run tree --chains 40 --seed 3 --topology tree --tas --algorithm BFDU
run ats --chains 40 --seed 4 --ats --classes 2 --algorithm FFDU
run strict --chains 40 --seed 8 --strict-let
run strict_ring --chains 40 --seed 9 --strict-let --topology ring --frer \
	--replicas 20 --classes 2 --algorithm CADU
run domain --config "$root/examples/templates/domain.json" --chains 40 \
	--seed 5
run distributed --config "$root/examples/templates/distributed.json" \
	--chains 30 --seed 6 --algorithm WFDU
run custom --config "$root/tests/netsim/custom.json" --chains 40 --seed 7 \
	--replicas 20
"$mapping" --example-application > "$dir/example.json"
run example "$dir/example.json"
