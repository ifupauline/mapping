#!/bin/sh
# The outputs shown in the manual, from the use cases of examples/.
# Usage: make_listings.sh <bin directory>
set -e
bin=$(cd "$1" && pwd)
here=$(cd "$(dirname "$0")" && pwd)
cd "$here/.."	# the paths printed are relative to the repository
ex=examples
m="$bin/mapping"
mkdir -p manual/listings
# the four parts of the output of UC1
"$m" $ex/uc1_central_sizing/application.json \
	$ex/uc1_central_sizing/configuration.json --cores > manual/listings/uc1_full.txt
sed -n '1,12p' manual/listings/uc1_full.txt > manual/listings/uc1_input.txt
sed -n '/metric/,/CPU time/p;/CPU time/{n;p}' manual/listings/uc1_full.txt \
	> manual/listings/uc1_summary.txt
sed -n '/cores used per ECU/,/^$/p' manual/listings/uc1_full.txt \
	| sed '/^$/d' > manual/listings/uc1_ecus.txt
sed -n '/cores in use/,/^$/p' manual/listings/uc1_full.txt | sed '/^$/d' \
	> manual/listings/uc1_cores.txt
sed -n '/task-chains$/,/^$/p' manual/listings/uc1_full.txt | sed '/^$/d' \
	> manual/listings/uc1_chains.txt
# UC1: the four rules, and the sizing
"$m" $ex/uc1_central_sizing/application.json \
	$ex/uc1_central_sizing/configuration.json --algorithm all \
	| grep -E '│ (metric|cores used M|critical scaling|task-chains meeting|inter-ECU|max reaction)' \
	> manual/listings/uc1_rules.txt
{
	printf '%-22s%14s%14s%14s%14s\n' "target" "cores" "cap found" "scaling" "on time (%)"
	for t in 0 1.5 2; do
		"$m" $ex/uc1_central_sizing/application.json \
			$ex/uc1_central_sizing/configuration.json --target-scaling $t \
			| python3 manual/table_rows.py "$t" "cores used M" "cap found" \
			"critical scaling" "task-chains meeting"
	done
} | sed 's/^0  /none/' > manual/listings/uc1_sizing.txt
# UC2: the four rules
"$m" $ex/uc2_redundant_chassis/application.json \
	$ex/uc2_redundant_chassis/configuration.json --algorithm all \
	| grep -E '│ (metric|cores used M|critical scaling|task-chains meeting|inter-ECU|  replicated with|network delay)' \
	> manual/listings/uc2_rules.txt
# UC2
"$m" $ex/uc2_redundant_chassis/application.json \
	$ex/uc2_redundant_chassis/configuration.json --cores > manual/listings/uc2_full.txt
sed -n '/task-chains$/,/^$/p' manual/listings/uc2_full.txt | sed '/^$/d' \
	> manual/listings/uc2_chains.txt
# UC3: one line per network setting
rows="python3 manual/table_rows.py"
{
	printf '%-22s%14s%14s%14s\n' "options" "on time (%)" "delay (us)" "port (%)"
	for f in "" "--ats" "--classes 1" "--link 100" "--link 100 --ats"; do
		# shellcheck disable=SC2086
		"$m" $ex/uc3_tsn_streams/application.json \
			$ex/uc3_tsn_streams/configuration.json $f \
			| $rows "${f:-(the file)}" "task-chains meeting" \
			"network delay bound" "busiest port"
	done
} > manual/listings/uc3_network.txt
# UC4: the SOP, then the updates on the frozen hardware, four rules
{
	printf '%-22s%14s%14s%14s%14s\n' "on time (%)" BFDU WFDU FFDU CADU
	for a in sop update1 update2; do
		"$m" $ex/uc4_software_updates/application_$a.json \
			$ex/uc4_software_updates/configuration_frozen.json \
			--algorithm all | $rows "$a" "task-chains meeting"
	done
} > manual/listings/uc4_updates.txt
"$m" $ex/uc4_software_updates/application_sop.json \
	$ex/uc4_software_updates/configuration_sop.json \
	| sed -n '/cores used per ECU/,/^$/p' | sed '/^$/d' > manual/listings/uc4_sop.txt
# UC5: with and without contention
{
	printf '%-22s%14s%14s%14s%14s\n' "memory latency" "cores" "load (cores)" "scaling" "on time (%)"
	for l in 0 40 100; do
		"$m" $ex/uc5_hybrid_platform/application.json \
			$ex/uc5_hybrid_platform/configuration.json --memory-latency $l \
			| $rows "$l ns" "cores used M" "application load" \
			"critical scaling" "task-chains meeting"
	done
} > manual/listings/uc5_contention.txt
"$m" $ex/uc5_hybrid_platform/application.json \
	$ex/uc5_hybrid_platform/configuration.json --cores \
	| sed -n '/cores in use/,/^$/p' | sed '/^$/d' \
	> manual/listings/uc5_cores.txt
# the help text
"$m" --help | sed "s|$bin/||" > manual/listings/help.txt
