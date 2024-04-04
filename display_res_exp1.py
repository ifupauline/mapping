"""
[0]    [1]
SIGMA, FR,

[2]     [3]           [4]           [5]           [6]             [7]
BFDU_M, BFDU_SR_ALLO, BFDU_SR_SWAP, BFDU_SR_DISP, BFDU_PLAC_GAIN, BFDU_LETU,

[8]     [9]           [10]          [11]          [12]            [13]
WFDU_M, WFDU_SR_ALLO, WFDU_SR_SWAP, WFDU_SR_DISP, WFDU_PLAC_GAIN, WFDU_LETU,

[14]    [15]          [16]          [17]          [18]            [19]
FFDU_M, FFDU_SR_ALLO, FFDU_SR_SWAP, FFDU_SR_DISP, FFDU_PLAC_GAIN, FFDU_LETU

"""
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D
import numpy as np
import pandas as pd

"print table"
df = pd.read_table("res_exp1.txt", sep=",")
print(df)

"load columns"
SIGMA = pd.read_table("res_exp1.txt", sep=",", usecols=[0])
FR = pd.read_table("res_exp1.txt", sep=",", usecols=[1])
BFDU_M = pd.read_table("res_exp1.txt", sep=",", usecols=[2])
BFDU_SR_ALLO = pd.read_table("res_exp1.txt", sep=",", usecols=[3])
BFDU_SR_SWAP = pd.read_table("res_exp1.txt", sep=",", usecols=[4])
BFDU_SR_DISP = pd.read_table("res_exp1.txt", sep=",", usecols=[5])
BFDU_PLAC_GAIN = pd.read_table("res_exp1.txt", sep=",", usecols=[6])
BFDU_LETU = pd.read_table("res_exp1.txt", sep=",", usecols=[7])
WFDU_M = pd.read_table("res_exp1.txt", sep=",", usecols=[8])
WFDU_SR_ALLO = pd.read_table("res_exp1.txt", sep=",", usecols=[9])
WFDU_SR_SWAP = pd.read_table("res_exp1.txt", sep=",", usecols=[10])
WFDU_SR_DISP = pd.read_table("res_exp1.txt", sep=",", usecols=[11])
WFDU_PLAC_GAIN = pd.read_table("res_exp1.txt", sep=",", usecols=[12])
WFDU_LETU = pd.read_table("res_exp1.txt", sep=",", usecols=[13])
FFDU_M = pd.read_table("res_exp1.txt", sep=",", usecols=[14])
FFDU_SR_ALLO = pd.read_table("res_exp1.txt", sep=",", usecols=[15])
FFDU_SR_SWAP = pd.read_table("res_exp1.txt", sep=",", usecols=[16])
FFDU_SR_DISP = pd.read_table("res_exp1.txt", sep=",", usecols=[17])
FFDU_PLAC_GAIN = pd.read_table("res_exp1.txt", sep=",", usecols=[18])
FFDU_LETU = pd.read_table("res_exp1.txt", sep=",", usecols=[19])

"sigma and fr"
plt.plot(SIGMA, FR)
plt.xlabel('SIGMA')
plt.ylabel('Fragmentation Rate')
plt.legend(["blue"], loc="upper right")
plt.show()

"fr and m"
plt.scatter(FR, BFDU_M)
plt.scatter(FR, WFDU_M)
plt.scatter(FR, FFDU_M)
plt.xlabel('Fragmentation Rate')
plt.ylabel('M')
plt.legend(["FFDU", "WFDU", "BFDU"], loc="upper right")
plt.show()

"sr and m"
plt.scatter(BFDU_SR_ALLO, BFDU_M)
plt.scatter(WFDU_SR_ALLO, WFDU_M)
plt.scatter(FFDU_SR_ALLO, FFDU_M)
plt.xlabel('Schedulability Rate ALLOCATION')
plt.ylabel('M')
plt.legend(["FFDU", "WFDU", "BFDU"], loc="upper right")
plt.show()

"sr and m"
plt.scatter(BFDU_SR_DISP, BFDU_M)
plt.scatter(WFDU_SR_DISP, WFDU_M)
plt.scatter(FFDU_SR_DISP, FFDU_M)
plt.xlabel('Schedulability Rate PLACEMENT')
plt.ylabel('M')
plt.legend(["FFDU", "WFDU", "BFDU"], loc="upper right")
plt.show()

"sr and let"
plt.scatter(BFDU_SR_ALLO, BFDU_LETU)
plt.scatter(WFDU_SR_ALLO, WFDU_LETU)
plt.scatter(FFDU_SR_ALLO, FFDU_LETU)
plt.xlabel('Schedulability Rate ALLOCATION')
plt.ylabel('LET Utilization')
plt.legend(["FFDU", "WFDU", "BFDU"], loc="upper right")
plt.show()

"sr and let"
plt.scatter(BFDU_SR_DISP, BFDU_LETU)
plt.scatter(WFDU_SR_DISP, WFDU_LETU)
plt.scatter(FFDU_SR_DISP, FFDU_LETU)
plt.xlabel('Schedulability Rate PLACEMENT')
plt.ylabel('LET Utilization')
plt.legend(["FFDU", "WFDU", "BFDU"], loc="upper right")
plt.show()

fig = plt.figure(figsize=(100,100))
ax = fig.add_subplot(111, projection='3d')
ax.scatter(BFDU_SR_ALLO, BFDU_M, BFDU_LETU)
ax.scatter(WFDU_SR_ALLO, WFDU_M, WFDU_LETU)
ax.scatter(FFDU_SR_ALLO, FFDU_M, FFDU_LETU)
ax.set_xlabel('Schedulability Rate ALLOCATION')
ax.set_ylabel('M')
ax.set_zlabel('LET Utilization')
plt.legend(["FFDU", "WFDU", "BFDU"], loc="upper right")
plt.show()

fig = plt.figure(figsize=(100,100))
ax = fig.add_subplot(111, projection='3d')
ax.scatter(BFDU_SR_DISP, BFDU_M, BFDU_LETU)
ax.scatter(WFDU_SR_DISP, WFDU_M, WFDU_LETU)
ax.scatter(FFDU_SR_DISP, FFDU_M, FFDU_LETU)
ax.set_xlabel('Schedulability Rate PLACEMENT')
ax.set_ylabel('M')
ax.set_zlabel('LET Utilization')
plt.legend(["FFDU", "WFDU", "BFDU"], loc="upper right")
plt.show()
