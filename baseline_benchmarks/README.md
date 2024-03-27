# Baseline benchmarks

## General Information
This directory contains the scripts for the automated execution of our baselines.

- In the [current directory](./), we provide 3 python scripts that are used to execute the YCSB workloads using the `redis` and `rocksdb` KV backends.
- In the [YCSB_baselines](./YCSB_baselines/) directory, we provide scripts that automate the execution of YCSB workloads using the `redis` KV backend through the YCSB Java Framework and `GDPRbench`. For more information, please see [here](./YCSB_baselines/README.md).

## Documentation

To run the benchmarks:
```
python3 baseline_runner.py
```
For more information about the execution configurations, see the [script](./baseline_runner.py) itself.

---

To plot the results:
```
python3 baseline_plot.py
```

---

Following we provide a breakdown with documentation about the scripts of the current directory:

- [`baseliner_runner.py`](./baseline_runner.py):
Contains the configuration and orchestrates the execution of the experiments.
  - The benchmark configuration is located in the first lines of the script.
  - The workloads are taken from the [workload_traces](../workload_traces/`) directory.
  - The number of repeats can be adapted directly in the script.
  - The `redis-server` is located in the [KVs/redis/src](../KVs/redis/src/) directory of the submodule.
  - The `rocksdb_server` is located in the [controller](../controller/) build directory that is created after building the controller.
  - The `localhost` is used for the communication.
  - The `ports` used for the servers are hardcoded in the script. Feel free to modify them according to your needs.
  - The results are placed in the `results` directory of the current directory (created, if it does not exist)

- [`aggregate_results.py`](./aggregate_results.py):
Contains helper functions to aggregate the results in files based on the amount of the repeats of each configuration.

- [`baseline_plot.py`](./baseline_plot.py):
Plots the aggregated results. 