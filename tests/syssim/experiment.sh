#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
# The experiment of the paper: bounds of the analysis against the
# full-system simulation, on several platforms and networks, five
# applications of 200 task-chains each; every observed/bound ratio into
# one CSV file.
# Usage: experiment.sh <mapping executable> <out.csv> [seeds (5)]
#	 [scenarios (4)]
set -e
mapping=$1
out=$2
seeds=${3:-5}
scenarios=${4:-4}
here=$(dirname "$0")
root=$here/../..
dir=$(mktemp -d)
trap 'rm -rf "$dir"' EXIT
echo "setting,seed,allocation,chain,tasks,inter_ecu_hops,end_to_end_bound_us,end_to_end_observed_us,reaction_bound_us,reaction_observed_us" > "$out"
run() {
	setting=$1
	seed=$2
	shift 2
	"$mapping" --chains 200 --seed "$seed" "$@" --out "$dir/m" > /dev/null
	python3 "$here/syssim.py" "$dir/m/system.json" "$scenarios" "$seed" \
		"$dir/r.csv" > "$dir/log.txt"
	cat "$dir/log.txt"
	! grep -q "violations = [1-9]" "$dir/log.txt"
	tail -n +2 "$dir/r.csv" | sed "s/^/$setting,$seed,/" >> "$out"
}
seed=1
while [ "$seed" -le "$seeds" ]; do
	run zonal-star "$seed" --algorithm FFDU
	run zonal-star-cadu "$seed" --algorithm CADU
	run zonal-ring-frer "$seed" --topology ring --frer --replicas 20 \
		--classes 3 --algorithm FFDU
	run zonal-contention "$seed" --memory-latency 20 --algorithm FFDU
	run zonal-tree-tas "$seed" --topology tree --tas --classes 3 \
		--algorithm FFDU
	run zonal-star-let "$seed" --strict-let --algorithm FFDU
	run zonal-ring-let "$seed" --strict-let --topology ring --frer \
		--replicas 20 --classes 3 --algorithm FFDU
	run domain "$seed" --config "$root/examples/templates/domain.json"
	run hybrid "$seed" --config "$root/examples/templates/hybrid.json"
	seed=$((seed + 1))
done
