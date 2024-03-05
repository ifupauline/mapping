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
plt.show()

"sigma and m"
plt.scatter(FR, BFDU_M)
plt.scatter(FR, WFDU_M)
plt.scatter(FR, FFDU_M)
plt.show()

"sigma and let"
plt.scatter(FR, BFDU_LETU)
plt.scatter(FR, WFDU_LETU)
plt.scatter(FR, FFDU_LETU)
plt.show()

"sigma and sr_allo"
plt.scatter(FR, BFDU_SR_ALLO)
plt.scatter(FR, WFDU_SR_ALLO)
plt.scatter(FR, FFDU_SR_ALLO)
plt.show()
