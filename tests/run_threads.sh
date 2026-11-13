#!/bin/sh
# The same experiment on 1 and on 2 threads must give the same CSV, CPU
# times (columns t_*) aside. Usage: run_threads.sh <experiment executable>
set -e
dir=$(mktemp -d)
trap 'rm -rf "$dir"' EXIT
"$1" let --instances 3 --jobs 1 --quiet --out "$dir/one.csv" > /dev/null
"$1" let --instances 3 --jobs 2 --quiet --out "$dir/two.csv" > /dev/null
# every row has as many fields as the header
if ! awk -F, 'NR == 1 { n = NF } NF != n { exit 1 }' "$dir/one.csv"; then
	echo "CSV rows and header have different numbers of fields"
	exit 1
fi
# keep the columns whose header does not start with t_
strip() {
	awk -F, 'NR == 1 { for (i = 1; i <= NF; i++) keep[i] = $i !~ /^t_/ }
		{ line = ""; for (i = 1; i <= NF; i++) if (keep[i])
			line = line $i ","; print line }' "$1"
}
strip "$dir/one.csv" > "$dir/one"
strip "$dir/two.csv" > "$dir/two"
if ! cmp -s "$dir/one" "$dir/two"; then
	echo "results differ between 1 and 2 threads:"
	diff "$dir/one" "$dir/two" | head -10
	exit 1
fi
echo "identical results on 1 and 2 threads ($(($(wc -l < "$dir/one") - 1)) rows)"
