# YCSB and GDPRbench workloads using YCSB Java Framework

### Vanilla redis
To run the `YCSB` workloads using the default `redis server` and the `YCSB redis client`, run:
```
./vanilla_redis_ycsb.sh
```
This script:
1. fetches and builds the `YCSB` framework
2. applies a patch to modify the workloads to use 100k records and 1M operations
3. fetches and builds `redis` (version 5.0.4) -- the [SDS.patch](./SDS.patch) is required for correct compilation
4. executes the workloads and stores the results in the `results` directory (created if it does not exist)

### GDPR-compliant redis
To run the YCSB and the GDPRbench workloads using the [patched](https://raw.githubusercontent.com/GDPRbench/redis-gdpr/master/src/gdpr-compliance.diff), GDPR-compliant redis server (provided by GDPRbench) and the GDPRbench redis client, run:
```
./gdprbench_redis_yscb.sh
```
This script:
1. fetches and builds the `GDPRbench` framework
2. applies a patch to modify the workloads to use 100k records and 1M operations
3. fetches, [patches]((https://raw.githubusercontent.com/GDPRbench/redis-gdpr/master/src/gdpr-compliance.diff)) (to make `redis` GDPR-compliant) and builds `redis` (version 5.0.4) -- the [SDS.patch](./SDS.patch) is required for correct compilation
4. executes the workloads and stores the results in the `results` directory (created if it does not exist)

### Plots

To produce the plots based on the results of the above executions, run:
```
python3 ycsb_plots.py
```
This python script is hardcoded to gather all the results from the `results` directory and produces 4 plots:
1. Runtime of the YCSB workloads for plain redis (using the YCSB redis client) and GDPR-compliant redis (using the GDPRbench redis client)
2. Throughput of the YCSB workloads for plain redis (using the YCSB redis client) and GDPR-compliant redis (using the GDPRbench redis client)
3. Runtime of the GDPRbench workloads for GDPR-compliant redis (using the GDPRbench redis client)
4. Throughput of the GDPRbench workloads for GDPR-compliant redis (using the GDPRbench redis client)

The plots are placed in the `plots` directory, which is created if it does not exist.

### Notes:
- The directory where `redis` is placing its files is hardcoded in the shell scripts. Modify it according to your needs.
- Each run is a dry-run which means that after each experiment `redis` is terminated, its stored files are deleted and a new instance is restarted.
- The waiting times that are required for the `sleeps` inside the shell script might need to be adapted depending on your machine. Otherwise you can use a more deterministic way to realise if `redis` has been terminated or if the server is active. For such an approach, you can see [here](../../evaluation/common.sh).
- The experiments use the `localhost` and the default `redis` port (6379).