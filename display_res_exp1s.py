"""
[0]    [1]
SIGMA, FR,

[2]     [3]           [4]           [5]           [6]             [7]        [8]
BFDU_M, BFDU_SR_ALLO, BFDU_SR_SWAP, BFDU_SR_DISP, BFDU_PLAC_GAIN, BFDU_LETU, BFDU_ET_TOOL,

[9]     [10]          [11]          [12]          [13]            [14]       [15]
WFDU_M, WFDU_SR_ALLO, WFDU_SR_SWAP, WFDU_SR_DISP, WFDU_PLAC_GAIN, WFDU_LETU, WFDU_ET_TOOL,

[16]    [17]          [18]          [19]          [20]            [21]       [22]
FFDU_M, FFDU_SR_ALLO, FFDU_SR_SWAP, FFDU_SR_DISP, FFDU_PLAC_GAIN, FFDU_LETU, FFDU_ET_TOOL,

"""
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D
import numpy as np
import pandas as pd

"print table"
df = pd.read_table("res_exp1s.txt", sep=",")
print(df)

"load columns"
SIGMA = pd.read_table("res_exp1s.txt", sep=",", usecols=[0])
FR = pd.read_table("res_exp1s.txt", sep=",", usecols=[1])
BFDU_M = pd.read_table("res_exp1s.txt", sep=",", usecols=[2])
BFDU_SR_ALLO = pd.read_table("res_exp1s.txt", sep=",", usecols=[3])
BFDU_SR_SWAP = pd.read_table("res_exp1s.txt", sep=",", usecols=[4])
BFDU_SR_DISP = pd.read_table("res_exp1s.txt", sep=",", usecols=[5])
BFDU_PLAC_GAIN = pd.read_table("res_exp1s.txt", sep=",", usecols=[6])
BFDU_LETU = pd.read_table("res_exp1s.txt", sep=",", usecols=[7])
BFDU_ET_TOOL = pd.read_table("res_exp1s.txt", sep=",", usecols=[8])
WFDU_M = pd.read_table("res_exp1s.txt", sep=",", usecols=[9])
WFDU_SR_ALLO = pd.read_table("res_exp1s.txt", sep=",", usecols=[10])
WFDU_SR_SWAP = pd.read_table("res_exp1s.txt", sep=",", usecols=[11])
WFDU_SR_DISP = pd.read_table("res_exp1s.txt", sep=",", usecols=[12])
WFDU_PLAC_GAIN = pd.read_table("res_exp1s.txt", sep=",", usecols=[13])
WFDU_LETU = pd.read_table("res_exp1s.txt", sep=",", usecols=[14])
WFDU_ET_TOOL = pd.read_table("res_exp1s.txt", sep=",", usecols=[15])
FFDU_M = pd.read_table("res_exp1s.txt", sep=",", usecols=[16])
FFDU_SR_ALLO = pd.read_table("res_exp1s.txt", sep=",", usecols=[17])
FFDU_SR_SWAP = pd.read_table("res_exp1s.txt", sep=",", usecols=[18])
FFDU_SR_DISP = pd.read_table("res_exp1s.txt", sep=",", usecols=[19])
FFDU_PLAC_GAIN = pd.read_table("res_exp1s.txt", sep=",", usecols=[20])
FFDU_LETU = pd.read_table("res_exp1s.txt", sep=",", usecols=[21])
FFDU_ET_TOOL = pd.read_table("res_exp1s.txt", sep=",", usecols=[22])

"sigma and fr"
plt.plot(SIGMA, FR)
plt.xlabel('SIGMA (U)')
plt.ylabel('FR (%)')
plt.show()

"sr and m"
plt.scatter(BFDU_SR_DISP, BFDU_M)
plt.scatter(WFDU_SR_DISP, WFDU_M)
plt.scatter(FFDU_SR_DISP, FFDU_M)
plt.xlabel('SR (%)')
plt.ylabel('M (number of cores in use)')
plt.legend(["FFDU", "WFDU", "BFDU"], loc="upper left")
plt.show()

"sr and let"
plt.scatter(BFDU_SR_DISP, BFDU_LETU)
plt.scatter(WFDU_SR_DISP, WFDU_LETU)
plt.scatter(FFDU_SR_DISP, FFDU_LETU)
plt.xlabel('SR (%)')
plt.ylabel('CC (%)')
plt.legend(["FFDU", "WFDU", "BFDU"], loc="upper left")
plt.show()

"fr and et"
plt.scatter(FR, BFDU_ET_TOOL)
plt.scatter(FR, WFDU_ET_TOOL)
plt.scatter(FR, FFDU_ET_TOOL)
plt.xlabel('FR (%)')
plt.ylabel('ET (ms)')
plt.legend(["FFDU", "WFDU", "BFDU"], loc="upper left")
plt.show()
