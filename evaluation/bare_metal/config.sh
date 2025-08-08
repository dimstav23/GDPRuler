# Variables for the end-to-end test configuration

# Variables for Direct client-server communication natively
direct_redis_address="127.0.0.1"
direct_redis_port=6379
direct_rocksdb_address="127.0.0.1"
direct_rocksdb_port=15001

# Variables for GDPR & Passthrough controller
ctl_redis_address="/tmp/redis.sock"
ctl_redis_port=0
ctl_rocksdb_address="/tmp/rocksdb.sock"
ctl_rocksdb_port=0
controller_address="127.0.0.1"
controller_port=1312

results_dir_suffix=""

# workload_type="large" # 10M ops
workload_type="medium" # 1M ops
# workload_type="small" # 1K ops

# Default combinations of
#   {1,2,4,8,16,32} clients,
#   {redis, rocksdb} dbs,
#   {workloada workloadb workloadc workloadd workloadf} workloads
clients="1 2 4 8 16"
dbs="redis rocksdb"
workloads="workloada_${workload_type} workloadb_${workload_type} workloadc_${workload_type} workloadd_${workload_type} workloadf_${workload_type}"
