import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D
import pandas as pd

"print table"
df = pd.read_table("res_exp2.txt", sep=",")
print(df)

"load columns"
SIGMA = pd.read_table("res_exp2.txt", sep=",", usecols=[0])
FR = pd.read_table("res_exp2.txt", sep=",", usecols=[1])
BFDU_M = pd.read_table("res_exp2.txt", sep=",", usecols=[2])
BFDU_SR_ALLO = pd.read_table("res_exp2.txt", sep=",", usecols=[3])
BFDU_SR_SWAP = pd.read_table("res_exp2.txt", sep=",", usecols=[4])
BFDU_SR_DISP = pd.read_table("res_exp2.txt", sep=",", usecols=[5])
BFDU_SWAP_GAIN = pd.read_table("res_exp2.txt", sep=",", usecols=[6])
BFDU_DISP_GAIN = pd.read_table("res_exp2.txt", sep=",", usecols=[7])
BFDU_PLAC_GAIN = pd.read_table("res_exp2.txt", sep=",", usecols=[8])
BFDU_LETU = pd.read_table("res_exp2.txt", sep=",", usecols=[9])
BFDU_ET_TOOL = pd.read_table("res_exp2.txt", sep=",", usecols=[10])
WFDU_M = pd.read_table("res_exp2.txt", sep=",", usecols=[11])
WFDU_SR_ALLO = pd.read_table("res_exp2.txt", sep=",", usecols=[12])
WFDU_SR_SWAP = pd.read_table("res_exp2.txt", sep=",", usecols=[13])
WFDU_SR_DISP = pd.read_table("res_exp2.txt", sep=",", usecols=[14])
WFDU_SWAP_GAIN = pd.read_table("res_exp2.txt", sep=",", usecols=[15])
WFDU_DISP_GAIN = pd.read_table("res_exp2.txt", sep=",", usecols=[16])
WFDU_PLAC_GAIN = pd.read_table("res_exp2.txt", sep=",", usecols=[17])
WFDU_LETU = pd.read_table("res_exp2.txt", sep=",", usecols=[18])
WFDU_ET_TOOL = pd.read_table("res_exp2.txt", sep=",", usecols=[19])
FFDU_M = pd.read_table("res_exp2.txt", sep=",", usecols=[20])
FFDU_SR_ALLO = pd.read_table("res_exp2.txt", sep=",", usecols=[21])
FFDU_SR_SWAP = pd.read_table("res_exp2.txt", sep=",", usecols=[22])
FFDU_SR_DISP = pd.read_table("res_exp2.txt", sep=",", usecols=[23])
FFDU_SWAP_GAIN = pd.read_table("res_exp2.txt", sep=",", usecols=[24])
FFDU_DISP_GAIN = pd.read_table("res_exp2.txt", sep=",", usecols=[25])
FFDU_PLAC_GAIN = pd.read_table("res_exp2.txt", sep=",", usecols=[26])
FFDU_LETU = pd.read_table("res_exp2.txt", sep=",", usecols=[27])
FFDU_ET_TOOL = pd.read_table("res_exp2.txt", sep=",", usecols=[28])

"sigma and fr"
plt.plot(SIGMA, FR, color = "black")
plt.xlabel('SIGMA')
plt.ylabel('FR (%)')
plt.show()

"fr and m"
plt.scatter(FR, BFDU_M, marker = "v", color = "#4f4f4f")
plt.scatter(FR, WFDU_M, color = "black")
plt.scatter(FR, FFDU_M, marker = "s", color = "silver")
plt.xlabel('FR (%)')
plt.ylabel('M (number of cores in use)')
plt.legend(["BFDU", "WFDU", "FFDU"], loc="lower right")
plt.show()

"m and let"
plt.scatter(BFDU_M, BFDU_LETU, marker = "v", color = "#4f4f4f")
plt.scatter(WFDU_M, WFDU_LETU, color = "black")
plt.scatter(FFDU_M, FFDU_LETU, marker = "s", color = "silver")
plt.xlabel('M (number of cores in use)')
plt.ylabel('CC (%)')
plt.legend(["BFDU", "WFDU", "FFDU"], loc="lower left")
plt.show()

"m and sr allo"
plt.scatter(BFDU_M, BFDU_SR_ALLO, marker = "v", color = "#4f4f4f")
plt.scatter(WFDU_M, WFDU_SR_ALLO, color = "black")
plt.scatter(FFDU_M, FFDU_SR_ALLO, marker = "s", color = "silver")
plt.xlabel('M (number of cores in use)')
plt.ylabel('SR (%)')
plt.legend(["BFDU", "WFDU", "FFDU"], loc="lower left")
plt.show()

"m and sr disp"
plt.scatter(BFDU_M, BFDU_SR_DISP, marker = "v", color = "#4f4f4f")
plt.scatter(WFDU_M, WFDU_SR_DISP, color = "black")
plt.scatter(FFDU_M, FFDU_SR_DISP, marker = "s", color = "silver")
plt.xlabel('M (number of cores in use)')
plt.ylabel('SR (%)')
plt.legend(["BFDU", "WFDU", "FFDU"], loc="lower center")
plt.show()

"m and sr disp_gain"
plt.scatter(BFDU_M, BFDU_PLAC_GAIN, marker = "v", color = "#4f4f4f")
plt.scatter(WFDU_M, WFDU_PLAC_GAIN, color = "black")
plt.scatter(FFDU_M, FFDU_PLAC_GAIN, marker = "s", color = "silver")
plt.xlabel('M (number of cores in use)')
plt.ylabel('SR (%)')
plt.legend(["BFDU", "WFDU", "FFDU"], loc="lower left")
plt.show()

"fr and sr disp"
plt.scatter(FR, BFDU_SR_DISP, marker = "v", color = "#4f4f4f")
plt.scatter(FR, WFDU_SR_DISP, color = "black")
plt.scatter(FR, FFDU_SR_DISP, marker = "s", color = "silver")
plt.xlabel('FR (%)')
plt.ylabel('SR (%)')
plt.ylim(90,100)
plt.legend(["BFDU", "WFDU", "FFDU"], loc="center right")
plt.show()

"fr and et"
plt.scatter(FR, BFDU_ET_TOOL, marker = "v", color = "#4f4f4f")
plt.scatter(FR, WFDU_ET_TOOL, color = "black")
plt.scatter(FR, FFDU_ET_TOOL, marker = "s", color = "silver")
plt.xlabel('FR (%)')
plt.ylabel('ET (ms)')
plt.legend(["BFDU", "WFDU", "FFDU"], loc="upper left")
plt.show()

"fr and sr swap gain"
plt.scatter(FR, BFDU_SWAP_GAIN, marker = "v", color = "#4f4f4f")
plt.scatter(FR, WFDU_SWAP_GAIN, color = "black")
plt.scatter(FR, FFDU_SWAP_GAIN, marker = "s", color = "silver")
plt.xlabel('FR (%)')
plt.ylabel('SR SWAP GAIN (%)')
plt.legend(["BFDU", "WFDU", "FFDU"], loc="lower right")
plt.show()

"fr and sr disp gain"
plt.scatter(FR, BFDU_DISP_GAIN, marker = "v", color = "#4f4f4f")
plt.scatter(FR, WFDU_DISP_GAIN, color = "black")
plt.scatter(FR, FFDU_DISP_GAIN, marker = "s", color = "silver")
plt.xlabel('FR (%)')
plt.ylabel('SR DISP GAIN (%)')
plt.legend(["BFDU", "WFDU", "FFDU"], loc="center right")
plt.show()

"fr and sr plac gain"
plt.scatter(FR, BFDU_PLAC_GAIN, marker = "v", color = "#4f4f4f")
plt.scatter(FR, WFDU_PLAC_GAIN, color = "black")
plt.scatter(FR, FFDU_PLAC_GAIN, marker = "s", color = "silver")
plt.xlabel('FR (%)')
plt.ylabel('SR PLAC GAIN (%)')
plt.legend(["BFDU", "WFDU", "FFDU"], loc="center right")
plt.show()
