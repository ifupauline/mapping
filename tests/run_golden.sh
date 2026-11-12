#!/bin/sh
# Compare the output of the golden generator with the committed golden
# results. Usage: run_golden.sh <golden executable> <committed directory>
set -e
out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT
"$1" "$out"
diff -r -u "$2" "$out" > "$out.diff" || {
	echo "golden results differ (first lines below, full diff in $out.diff):"
	head -40 "$out.diff"
	exit 1
}
echo "golden results identical ($(ls "$2" | wc -l) files)"
