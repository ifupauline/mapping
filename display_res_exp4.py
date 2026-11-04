import os
import sys

import matplotlib

"usage: python3 display_res_exp4.py [results_file] [--save [output_dir]]"
"  --save writes every figure as a PNG instead of opening windows"
ARGS = [a for a in sys.argv[1:]]
SAVE = "--save" in ARGS
OUT = "figures"
if SAVE:
    i = ARGS.index("--save")
    if i + 1 < len(ARGS) and not ARGS[i + 1].endswith(".txt"):
        OUT = ARGS.pop(i + 1)
    ARGS.remove("--save")
    matplotlib.use("Agg")
RES = ARGS[0] if ARGS else "res_exp4.txt"
if not os.path.exists(RES):
    sys.exit("results file not found: " + RES + " (run ./bin/exp4 first, from the same folder)")

import matplotlib.pyplot as plt
import pandas as pd


def show(name):
    "open the figure, or save it as <OUT>/exp4_<name>.png with --save"
    if SAVE:
        os.makedirs(OUT, exist_ok=True)
        path = os.path.join(OUT, "exp4_" + name + ".png")
        plt.savefig(path, dpi=150, bbox_inches="tight")
        plt.close()
        print("saved", path)
    else:
        plt.show()


"columns by name (header names have stray spaces)"
COLS = pd.read_table(RES, sep=",")
COLS.columns = COLS.columns.str.strip()

"print table"
df = pd.read_table(RES, sep=",")
print(df)

"load columns"
m = pd.read_table(RES, sep=",", usecols=[0])
GROWTH_RATE = pd.read_table(RES, sep=",", usecols=[1])
ET_PART = pd.read_table(RES, sep=",", usecols=[2])
ET_PART_LINE = pd.read_table(RES, sep=",", usecols=[3])
ET_PART_QUAD = pd.read_table(RES, sep=",", usecols=[4])
ET_ASSI = pd.read_table(RES, sep=",", usecols=[5])
ET_ASSI_LINE = pd.read_table(RES, sep=",", usecols=[6])
ET_ASSI_QUAD = pd.read_table(RES, sep=",", usecols=[7])
ET_SCHD = pd.read_table(RES, sep=",", usecols=[8])
ET_SCHD_LINE = pd.read_table(RES, sep=",", usecols=[9])
ET_SCHD_QUAD = pd.read_table(RES, sep=",", usecols=[10])
ET_SWAP = pd.read_table(RES, sep=",", usecols=[11])
ET_SWAP_LINE = pd.read_table(RES, sep=",", usecols=[12])
ET_SWAP_QUAD = pd.read_table(RES, sep=",", usecols=[13])
ET_DISP = pd.read_table(RES, sep=",", usecols=[14])
ET_DISP_LINE = pd.read_table(RES, sep=",", usecols=[15])
ET_DISP_QUAD = pd.read_table(RES, sep=",", usecols=[16])
ET_TOOL = pd.read_table(RES, sep=",", usecols=[17])
ET_TOOL_LINE = pd.read_table(RES, sep=",", usecols=[18])
ET_TOOL_QUAD = pd.read_table(RES, sep=",", usecols=[19])

"et_part and quad"
plt.plot(m, ET_PART, color = "black")
plt.plot(m, ET_PART_LINE, linestyle = "dashed", color = "#4f4f4f")
plt.xlabel('m')
plt.ylabel('ET ms')
plt.legend(["ET PART","(O)n"], loc="upper left")
show("et_part_and_quad")

"et_assi and quad"
plt.plot(m, ET_ASSI, color = "black")
plt.plot(m, ET_ASSI_LINE, linestyle = "dashed", color = "#4f4f4f")
plt.plot(m, ET_ASSI_QUAD, linestyle = "dashed", color = "silver")
plt.xlabel('m')
plt.ylabel('ET ms')
plt.legend(["ET ASSI","(O)n", "(O)n^2"], loc="upper left")
show("et_assi_and_quad")

"et_schd and quad"
plt.plot(m, ET_SCHD, color = "black")
plt.plot(m, ET_SCHD_LINE, linestyle = "dashed", color = "#4f4f4f")
plt.xlabel('m')
plt.ylabel('ET ms')
plt.legend(["ET SCHD","(O)n"], loc="upper left")
show("et_schd_and_quad")

"et_swap and quad"
plt.plot(m, ET_SWAP, color = "black")
plt.plot(m, ET_SWAP_LINE, linestyle = "dashed", color = "#4f4f4f")
plt.plot(m, ET_SWAP_QUAD, linestyle = "dashed", color = "silver")
plt.xlabel('m')
plt.ylabel('ET ms')
plt.legend(["ET SWAP","(O)n", "(O)n^2"], loc="upper left")
show("et_swap_and_quad")

"et_disp and quad"
plt.plot(m, ET_DISP, color = "black")
plt.plot(m, ET_DISP_LINE, linestyle = "dashed", color = "#4f4f4f")
plt.plot(m, ET_DISP_QUAD, linestyle = "dashed", color = "silver")
plt.xlabel('m')
plt.ylabel('ET ms')
plt.legend(["ET DISP","(O)n", "(O)n^2"], loc="upper left")
show("et_disp_and_quad")

"et_tool and quad"
plt.plot(m, ET_TOOL, color = "black")
plt.plot(m, ET_TOOL_LINE, linestyle = "dashed", color = "#4f4f4f")
plt.plot(m, ET_TOOL_QUAD, linestyle = "dashed", color = "silver")
plt.xlabel('m')
plt.ylabel('ET ms')
plt.legend(["ET TOOL","(O)n", "(O)n^2"], loc="upper left")
show("et_tool_and_quad")

"et_comm and quad"
if "ET_COMM" in COLS:
    plt.plot(COLS["m"], COLS["ET_COMM"], color="black")
    plt.plot(COLS["m"], COLS["ET_COMM_LINE"], linestyle="dashed", color="#4f4f4f")
    plt.plot(COLS["m"], COLS["ET_COMM_QUAD"], linestyle="dashed", color="silver")
    plt.xlabel("m")
    plt.ylabel("ET ms")
    plt.legend(["ET COMM", "(O)n", "(O)n^2"], loc="upper left")
    show("et_comm_and_quad")

    "m and communications"
    plt.plot(COLS["m"], COLS["IC"], color="black")
    plt.plot(COLS["m"], COLS["IE"], linestyle="dashed", color="#4f4f4f")
    plt.xlabel("m")
    plt.ylabel("number of communications")
    plt.legend(["inter-core", "inter-ECU"], loc="upper left")
    show("m_and_communications")
