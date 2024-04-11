"""
[0]    [1]
m,     ET_PART,

[2]      [3]           [4]           [5]           [6]
ET_ASSI, ET_SCHD,      ET_SWAP,      ET_DISP,      ET_TOOL

"""
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D
import pandas as pd

"print table"
df = pd.read_table("res_exp4.txt", sep=",")
print(df)

"load columns"
m = pd.read_table("res_exp2.txt", sep=",", usecols=[0])
ET_PART = pd.read_table("res_exp4.txt", sep=",", usecols=[1])
ET_ASSI = pd.read_table("res_exp4.txt", sep=",", usecols=[2])
ET_SCHD = pd.read_table("res_exp4.txt", sep=",", usecols=[3])
ET_SWAP = pd.read_table("res_exp4.txt", sep=",", usecols=[4])
ET_DISP = pd.read_table("res_exp4.txt", sep=",", usecols=[5])
ET_TOOL = pd.read_table("res_exp4.txt", sep=",", usecols=[6])
ET_QUAD = pd.read_table("res_exp4.txt", sep=",", usecols=[7])

"m and et_tool"
plt.plot(m, ET_TOOL)
plt.plot(m, ET_QUAD)
plt.xlabel('m')
plt.ylabel('Tool Execution Time Rate')
plt.legend(["blue"], loc="upper right")
plt.show()
