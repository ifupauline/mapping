import os
import sys

import pandas as pd

"usage: python3 display_res_exp3.py [results_file]"
"Experiment 3 is a single design point: print it as Table 4.4"
RES = sys.argv[1] if len(sys.argv) > 1 else "res_exp3.txt"
if not os.path.exists(RES):
    sys.exit("results file not found: " + RES + " (run ./bin/exp3 first, from the same folder)")

COLS = pd.read_table(RES, sep=",")
COLS.columns = COLS.columns.str.strip()
row = COLS.iloc[-1]

print("Execution times of the tool (seconds), Table 4.4")
table = pd.DataFrame([{
    "tc count": int(row["TC_COUNT"]),
    "task count": int(row["TASK_COUNT"]),
    "PA count": int(row["PA_COUNT"]),
    "ET part": row["ET_PART"] / 1000,
    "ET assi": row["ET_ASSI"] / 1000,
    "ET swap": row["ET_SWAP"] / 1000,
    "ET disp": row["ET_DISP"] / 1000,
    "ET tool": row["ET_TOOL"] / 1000,
}])
print(table.to_string(index=False))

if "IC" in COLS:
    print()
    print("Communication")
    comm = pd.DataFrame([{
        "inter-core": int(row["IC"]),
        "inter-ECU": int(row["IE"]),
        "SR task-chains (%)": row["SR_TC"],
        "avg e2e (us)": row["E2E_AVG"],
        "max e2e (us)": row["E2E_MAX"],
        "ET comm (s)": row["ET_COMM"] / 1000,
    }])
    print(comm.to_string(index=False))

if "NET_MAX" in COLS:
    print()
    print("TSN network (Network Calculus bounds)")
    net = pd.DataFrame([{
        "avg delay bound (us)": row["NET_AVG"],
        "max delay bound (us)": row["NET_MAX"],
        "max port load (%)": row["NET_UTIL"],
    }])
    print(net.to_string(index=False))
