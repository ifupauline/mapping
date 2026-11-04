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
