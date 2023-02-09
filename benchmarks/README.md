## Description of [`redis_runner.sh`](./redis_runner.sh)

This script measures the performance of three different versions of a Redis server:

Vanilla Native Version: A standard Redis server without any privacy controls
GDPRuler Native Version: A Redis server with privacy controls, using the GDPRuler tool
SEV GDPRuler Version: A secure version of the GDPRuler Native Version.
The script measures the performance of each version of the Redis server by running a set of five workload traces and measuring the "Controller Time" and "System Time". The results of each measurement are then averaged and stored in a CSV file.

Here is a high-level description of the steps taken by the script:

The script sets a number of variables, including the path to the Redis server and the workload traces, the number of repeats (5), and the names of the workload traces ("workloada", "workloadb", "workloadc", "workloadd", and "workloadf").
The script creates a directory to store the results, if it doesn't already exist.
The script runs a loop to measure the performance of the Vanilla Native Version. In each iteration of the loop:
The Redis server is started and its process ID is stored
The workload trace is run using the python3 ../native_ctl.py command, which runs the "native_ctl.py" script with the specified workload. The output of the command is redirected to a file with the name "./results/native_${workload}_${rep}"
The script extracts the "Controller Time" and "System Time" from the output file and adds them to the running total of times.
The Redis server is killed and its data file is removed.
The script calculates the average "Controller Time" and "System Time" for the Vanilla Native Version and writes the results to the "./results/native_average.csv" file.
The script repeats steps 3 and 4 for the GDPRuler Native Version, with the difference being that the python3 ../GDPRuler.py command is used to run the "GDPRuler.py" script with the specified configuration file and workload. The results are stored in the "./results/gdpr_average.csv" file.
The script repeats steps 3 and 4 for the SEV GDPRuler Version, with the same process as the GDPRuler Native Version. The results are stored in the "./results/sec_gdpr_average.csv" file.
This script uses a number of Linux shell commands and utilities, including mkdir to create the results directory, seq to generate a sequence of numbers, grep to extract information from the output files, awk to extract values from the output of grep, bc to perform floating-point arithmetic, and kill to kill the Redis server process.