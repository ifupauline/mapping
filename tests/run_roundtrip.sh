#!/bin/sh
# The input files reproduce the built-in run: an application generated with
# some options and written with --generate, and the zonal configuration
# written with --print-config, give the same results as the generator and
# the options themselves (CPU times aside).
# Usage: run_roundtrip.sh <mapping executable>
set -e
mapping=$1
out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT
metrics() {
	sed -n "/metric/,\$p" "$1" | grep -v -e "CPU time" -e "results written" -e "^$"
}
check() {
	app_options=$1
	method_options=$2
	# shellcheck disable=SC2086
	"$mapping" $app_options $method_options > "$out/builtin.txt"
	# shellcheck disable=SC2086
	"$mapping" $app_options --generate > "$out/application.json"
	# shellcheck disable=SC2086
	"$mapping" $method_options --algorithm all --print-config \
		> "$out/configuration.json"
	"$mapping" "$out/application.json" "$out/configuration.json" \
		--algorithm all --out "$out/results" > "$out/files.txt"
	metrics "$out/builtin.txt" > "$out/builtin.metrics"
	metrics "$out/files.txt" > "$out/files.metrics"
	if ! cmp -s "$out/builtin.metrics" "$out/files.metrics"; then
		echo "different results for: $app_options $method_options"
		diff "$out/builtin.metrics" "$out/files.metrics" | head -20
		exit 1
	fi
	for f in mapping.csv chains.csv cores.csv ecus.csv summary.json; do
		test -s "$out/results/$f" || { echo "missing $f"; exit 1; }
	done
	echo "same results: $app_options $method_options"
}
check "--seed 3 --chains 150" ""
check "--seed 7 --chains 200 --replicas 20 --zone-only 20 --lockstep 10 --type-variation 30 --colocate 20" \
	"--memory-latency 20 --topology ring --frer --classes 3"
check "--seed 5 --chains 120" \
	"--pcu-cores 6 --pcu-little 2 --zcu-lockstep 1 --zcu-memory 4000 --topology tree --tas --sigma 0.2"
check "--detection-chain" "--ats --classes 2"
