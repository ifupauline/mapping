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
plt.plot(SIGMA, FR, color = "black")
plt.xlabel('SIGMA')
plt.ylabel('FR (%)')
plt.show()

"sigma and sr"
plt.scatter(SIGMA, BFDU_SR_DISP, marker = "v", color = "#4f4f4f")
plt.scatter(SIGMA, WFDU_SR_DISP, color = "black")
plt.scatter(SIGMA, FFDU_SR_DISP, marker = "s", color = "silver")
plt.xlabel('SIGMA')
plt.ylabel('SR (%)')
plt.legend(["BFDU", "WFDU", "FFDU"], loc="upper left")
plt.show()

"sigma and m"
plt.scatter(SIGMA, BFDU_M, marker = "v", color = "#4f4f4f")
plt.scatter(SIGMA, WFDU_M, color = "black")
plt.scatter(SIGMA, FFDU_M, marker = "s", color = "silver")
plt.xlabel('SIGMA')
plt.ylabel('M (number of cores in use)')
plt.legend(["BFDU", "WFDU", "FFDU"], loc="center right")
plt.show()

"sigma and let"
plt.scatter(SIGMA, BFDU_LETU, marker = "v", color = "#4f4f4f")
plt.scatter(SIGMA, WFDU_LETU, color = "black")
plt.scatter(SIGMA, FFDU_LETU, marker = "s", color = "silver")
plt.xlabel('SIGMA')
plt.ylabel('CC (%)')
plt.legend(["BFDU", "WFDU", "FFDU"], loc="upper right")
plt.show()

"m and let"
plt.scatter(BFDU_M, BFDU_LETU, marker = "v", color = "#4f4f4f")
plt.scatter(WFDU_M, WFDU_LETU, color = "black")
plt.scatter(FFDU_M, FFDU_LETU, marker = "s", color = "silver")
plt.xlabel('M')
plt.ylabel('CC (%)')
plt.legend(["BFDU", "WFDU", "FFDU"], loc="upper right")
plt.show()

"fr and et"
plt.scatter(FR, BFDU_ET_TOOL, marker = "v", color = "#4f4f4f")
plt.scatter(FR, WFDU_ET_TOOL, color = "black")
plt.scatter(FR, FFDU_ET_TOOL, marker = "s", color = "silver")
plt.xlabel('FR (%)')
plt.ylabel('ET (ms)')
plt.legend(["BFDU", "WFDU", "FFDU"], loc="upper left")
plt.show()
