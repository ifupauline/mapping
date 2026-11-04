import os
import sys

import matplotlib

"usage: python3 display_res_exp2.py [results_file] [--save [output_dir]]"
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
RES = ARGS[0] if ARGS else "res_exp2.txt"
if not os.path.exists(RES):
    sys.exit("results file not found: " + RES + " (run ./bin/exp2 first, from the same folder)")

import matplotlib.pyplot as plt
import pandas as pd


def show(name):
    "open the figure, or save it as <OUT>/exp2_<name>.png with --save"
    if SAVE:
        os.makedirs(OUT, exist_ok=True)
        path = os.path.join(OUT, "exp2_" + name + ".png")
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
SIGMA = pd.read_table(RES, sep=",", usecols=[0])
FR = pd.read_table(RES, sep=",", usecols=[1])
BFDU_M = pd.read_table(RES, sep=",", usecols=[2])
BFDU_SR_ALLO = pd.read_table(RES, sep=",", usecols=[3])
BFDU_SR_SWAP = pd.read_table(RES, sep=",", usecols=[4])
BFDU_SR_DISP = pd.read_table(RES, sep=",", usecols=[5])
BFDU_SWAP_GAIN = pd.read_table(RES, sep=",", usecols=[6])
BFDU_DISP_GAIN = pd.read_table(RES, sep=",", usecols=[7])
BFDU_PLAC_GAIN = pd.read_table(RES, sep=",", usecols=[8])
BFDU_LETU = pd.read_table(RES, sep=",", usecols=[9])
BFDU_ET_TOOL = pd.read_table(RES, sep=",", usecols=[10])
WFDU_M = pd.read_table(RES, sep=",", usecols=[11])
WFDU_SR_ALLO = pd.read_table(RES, sep=",", usecols=[12])
WFDU_SR_SWAP = pd.read_table(RES, sep=",", usecols=[13])
WFDU_SR_DISP = pd.read_table(RES, sep=",", usecols=[14])
WFDU_SWAP_GAIN = pd.read_table(RES, sep=",", usecols=[15])
WFDU_DISP_GAIN = pd.read_table(RES, sep=",", usecols=[16])
WFDU_PLAC_GAIN = pd.read_table(RES, sep=",", usecols=[17])
WFDU_LETU = pd.read_table(RES, sep=",", usecols=[18])
WFDU_ET_TOOL = pd.read_table(RES, sep=",", usecols=[19])
FFDU_M = pd.read_table(RES, sep=",", usecols=[20])
FFDU_SR_ALLO = pd.read_table(RES, sep=",", usecols=[21])
FFDU_SR_SWAP = pd.read_table(RES, sep=",", usecols=[22])
FFDU_SR_DISP = pd.read_table(RES, sep=",", usecols=[23])
FFDU_SWAP_GAIN = pd.read_table(RES, sep=",", usecols=[24])
FFDU_DISP_GAIN = pd.read_table(RES, sep=",", usecols=[25])
FFDU_PLAC_GAIN = pd.read_table(RES, sep=",", usecols=[26])
FFDU_LETU = pd.read_table(RES, sep=",", usecols=[27])
FFDU_ET_TOOL = pd.read_table(RES, sep=",", usecols=[28])

"sigma and fr"
plt.plot(SIGMA, FR, color = "black")
plt.xlabel('SIGMA')
plt.ylabel('FR (%)')
show("sigma_and_fr")

"fr and m"
plt.scatter(FR, BFDU_M, marker = "v", color = "#4f4f4f")
plt.scatter(FR, WFDU_M, color = "black")
plt.scatter(FR, FFDU_M, marker = "s", color = "silver")
plt.xlabel('FR (%)')
plt.ylabel('M (number of cores in use)')
plt.legend(["BFDU", "WFDU", "FFDU"], loc="lower right")
show("fr_and_m")

"m and let"
plt.scatter(BFDU_M, BFDU_LETU, marker = "v", color = "#4f4f4f")
plt.scatter(WFDU_M, WFDU_LETU, color = "black")
plt.scatter(FFDU_M, FFDU_LETU, marker = "s", color = "silver")
plt.xlabel('M (number of cores in use)')
plt.ylabel('CC (%)')
plt.legend(["BFDU", "WFDU", "FFDU"], loc="lower left")
show("m_and_let")

"m and sr allo"
plt.scatter(BFDU_M, BFDU_SR_ALLO, marker = "v", color = "#4f4f4f")
plt.scatter(WFDU_M, WFDU_SR_ALLO, color = "black")
plt.scatter(FFDU_M, FFDU_SR_ALLO, marker = "s", color = "silver")
plt.xlabel('M (number of cores in use)')
plt.ylabel('SR (%)')
plt.legend(["BFDU", "WFDU", "FFDU"], loc="lower left")
show("m_and_sr_allo")

"m and sr disp"
plt.scatter(BFDU_M, BFDU_SR_DISP, marker = "v", color = "#4f4f4f")
plt.scatter(WFDU_M, WFDU_SR_DISP, color = "black")
plt.scatter(FFDU_M, FFDU_SR_DISP, marker = "s", color = "silver")
plt.xlabel('M (number of cores in use)')
plt.ylabel('SR (%)')
plt.legend(["BFDU", "WFDU", "FFDU"], loc="lower center")
show("m_and_sr_disp")

"m and sr disp_gain"
plt.scatter(BFDU_M, BFDU_PLAC_GAIN, marker = "v", color = "#4f4f4f")
plt.scatter(WFDU_M, WFDU_PLAC_GAIN, color = "black")
plt.scatter(FFDU_M, FFDU_PLAC_GAIN, marker = "s", color = "silver")
plt.xlabel('M (number of cores in use)')
plt.ylabel('SR (%)')
plt.legend(["BFDU", "WFDU", "FFDU"], loc="lower left")
show("m_and_sr_disp_gain")

"fr and sr disp"
plt.scatter(FR, BFDU_SR_DISP, marker = "v", color = "#4f4f4f")
plt.scatter(FR, WFDU_SR_DISP, color = "black")
plt.scatter(FR, FFDU_SR_DISP, marker = "s", color = "silver")
plt.xlabel('FR (%)')
plt.ylabel('SR (%)')
plt.ylim(90,100)
plt.legend(["BFDU", "WFDU", "FFDU"], loc="center right")
show("fr_and_sr_disp")

"fr and et"
plt.scatter(FR, BFDU_ET_TOOL, marker = "v", color = "#4f4f4f")
plt.scatter(FR, WFDU_ET_TOOL, color = "black")
plt.scatter(FR, FFDU_ET_TOOL, marker = "s", color = "silver")
plt.xlabel('FR (%)')
plt.ylabel('ET (ms)')
plt.legend(["BFDU", "WFDU", "FFDU"], loc="upper left")
show("fr_and_et")

"fr and sr swap gain"
plt.scatter(FR, BFDU_SWAP_GAIN, marker = "v", color = "#4f4f4f")
plt.scatter(FR, WFDU_SWAP_GAIN, color = "black")
plt.scatter(FR, FFDU_SWAP_GAIN, marker = "s", color = "silver")
plt.xlabel('FR (%)')
plt.ylabel('SR SWAP GAIN (%)')
plt.legend(["BFDU", "WFDU", "FFDU"], loc="lower right")
show("fr_and_sr_swap_gain")

"fr and sr disp gain"
plt.scatter(FR, BFDU_DISP_GAIN, marker = "v", color = "#4f4f4f")
plt.scatter(FR, WFDU_DISP_GAIN, color = "black")
plt.scatter(FR, FFDU_DISP_GAIN, marker = "s", color = "silver")
plt.xlabel('FR (%)')
plt.ylabel('SR DISP GAIN (%)')
plt.legend(["BFDU", "WFDU", "FFDU"], loc="center right")
show("fr_and_sr_disp_gain")

"fr and sr plac gain"
plt.scatter(FR, BFDU_PLAC_GAIN, marker = "v", color = "#4f4f4f")
plt.scatter(FR, WFDU_PLAC_GAIN, color = "black")
plt.scatter(FR, FFDU_PLAC_GAIN, marker = "s", color = "silver")
plt.xlabel('FR (%)')
plt.ylabel('SR PLAC GAIN (%)')
plt.legend(["BFDU", "WFDU", "FFDU"], loc="center right")
show("fr_and_sr_plac_gain")

"--- communication (columns appended after the original ones) ---"
ALGOS = [("BFDU", "v", "#4f4f4f"), ("WFDU", "o", "black"), ("FFDU", "s", "silver")]


def scatter_all(x, y_suffix, xlabel, ylabel, loc="upper left"):
    for a, mk, col in ALGOS:
        xs = COLS[x] if x in COLS else COLS[a + "_" + x]
        plt.scatter(xs, COLS[a + "_" + y_suffix], marker=mk, color=col)
    plt.xlabel(xlabel)
    plt.ylabel(ylabel)
    plt.legend([a for a, _, _ in ALGOS], loc=loc)


if "FFDU_IC" in COLS:
    "FR and communications"
    for a, mk, col in ALGOS:
        plt.scatter(COLS["FR"], COLS[a + "_IC"] + COLS[a + "_IE"], marker=mk, color=col)
    plt.xlabel("FR (%)")
    plt.ylabel("number of communications (inter-core + inter-ECU)")
    plt.legend([a for a, _, _ in ALGOS], loc="upper left")
    show("fr_and_communications")

    "FR and inter-ecu communications"
    scatter_all("FR", "IE", "FR (%)", "number of inter-ECU communications", "upper left")
    show("fr_and_inter_ecu_communications")

    "FR and task-chain schedulability"
    scatter_all("FR", "SR_TC", "FR (%)", "SR task-chains (%)", "lower left")
    show("fr_and_sr_tc")

    "fr and e2e latency"
    scatter_all("FR", "E2E_AVG", "FR (%)", "average e2e latency (us)")
    show("fr_and_e2e_avg")

    "fr and max e2e latency"
    scatter_all("FR", "E2E_MAX", "FR (%)", "max e2e latency (us)")
    show("fr_and_e2e_max")

    "m and e2e latency"
    scatter_all("M", "E2E_AVG", "M (number of cores in use)", "average e2e latency (us)", "upper right")
    show("m_and_e2e_avg")
