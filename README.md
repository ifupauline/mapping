cmake .
make

./bin/exp1
./bin/exp2
./bin/exp3
./bin/exp4
./bin/pa
./bin/map 500      (sigma in permill, 150 <= sigma <= 800)

Some experiments may take up to one hour.

Each experiment writes its results (res_exp1.txt ... res_exp4.txt) in the
folder it is launched from. The last columns of each file are the
communication metrics: inter-core and inter-ECU communications, task-chain
schedulability, average and max end-to-end latency, communication time.

Plot the results (from the same folder):

pip install -r requirements.txt

python3 display_res_exp1.py                 # opens the figures one by one
python3 display_res_exp2.py --save          # writes them to figures/*.png
python3 display_res_exp3.py                 # prints Table 4.4
python3 display_res_exp4.py path/to/res_exp4.txt --save out/

Network model (inter-controller messages, src/network.cpp)

Star topology: the PCU and the 5 ZCUs each have one full-duplex 1 Gbit/s
Ethernet link to a central TSN switch. LET messages use the highest IEEE
802.1Q priority class, shaped by the 802.1Qav Credit-Based Shaper (idle
slope 75%); best-effort traffic is assumed always present with maximum size
frames (no frame preemption). The transfer time of every inter-ECU message
is bounded with Network Calculus (token-bucket flows, CBS rate-latency
service curves, per-port delay bounds with burst propagation) and added to
the end-to-end latency of its task-chain. Parameters (link rate, idle slope,
switch latency, message sizes) are defined in include/network.h.
