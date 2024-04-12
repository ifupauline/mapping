import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D
import pandas as pd

"print table"
df = pd.read_table("res_exp4.txt", sep=",")
print(df)

"load columns"
m = pd.read_table("res_exp4.txt", sep=",", usecols=[0])
GROWTH_RATE = pd.read_table("res_exp4.txt", sep=",", usecols=[1])
ET_PART = pd.read_table("res_exp4.txt", sep=",", usecols=[2])
ET_PART_LINE = pd.read_table("res_exp4.txt", sep=",", usecols=[3])
ET_PART_QUAD = pd.read_table("res_exp4.txt", sep=",", usecols=[4])
ET_ASSI = pd.read_table("res_exp4.txt", sep=",", usecols=[5])
ET_ASSI_LINE = pd.read_table("res_exp4.txt", sep=",", usecols=[6])
ET_ASSI_QUAD = pd.read_table("res_exp4.txt", sep=",", usecols=[7])
ET_SWAP = pd.read_table("res_exp4.txt", sep=",", usecols=[8])
ET_SWAP_LINE = pd.read_table("res_exp4.txt", sep=",", usecols=[9])
ET_SWAP_QUAD = pd.read_table("res_exp4.txt", sep=",", usecols=[10])
ET_DISP = pd.read_table("res_exp4.txt", sep=",", usecols=[11])
ET_DISP_LINE = pd.read_table("res_exp4.txt", sep=",", usecols=[12])
ET_DISP_QUAD = pd.read_table("res_exp4.txt", sep=",", usecols=[13])
ET_TOOL = pd.read_table("res_exp4.txt", sep=",", usecols=[14])
ET_TOOL_LINE = pd.read_table("res_exp4.txt", sep=",", usecols=[15])
ET_TOOL_QUAD = pd.read_table("res_exp4.txt", sep=",", usecols=[16])

"et_part and quad"
plt.plot(m, ET_PART)
plt.plot(m, ET_PART_LINE)
plt.xlabel('m')
plt.ylabel('ET ms')
plt.legend(["ET PART","(O)n", "(O)n^2"], loc="upper left")
plt.show()

"et_assi and quad"
plt.plot(m, ET_ASSI)
plt.plot(m, ET_ASSI_LINE)
plt.xlabel('m')
plt.ylabel('ET ms')
plt.legend(["ET ASSI","(O)n", "(O)n^2"], loc="upper left")
plt.show()

"et_swap and quad"
plt.plot(m, ET_SWAP)
plt.plot(m, ET_SWAP_LINE)
plt.xlabel('m')
plt.ylabel('ET ms')
plt.legend(["ET SWAP","(O)n", "(O)n^2"], loc="upper left")
plt.show()

"et_disp and quad"
plt.plot(m, ET_DISP)
plt.plot(m, ET_DISP_LINE)
plt.xlabel('m')
plt.ylabel('ET ms')
plt.legend(["ET DISP","(O)n", "(O)n^2"], loc="upper left")
plt.show()

"et_tool and quad"
plt.plot(m, ET_TOOL)
plt.plot(m, ET_TOOL_LINE)
plt.plot(m, ET_TOOL_QUAD)
plt.xlabel('m')
plt.ylabel('ET ms')
plt.legend(["ET TOOL","(O)n", "(O)n^2"], loc="upper left")
plt.show()
