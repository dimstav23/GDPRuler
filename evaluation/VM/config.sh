# Variables for the end-to-end test configuration

# Variables for Direct client-server communication in the CVM
direct_redis_address="192.168.122.48"
direct_redis_port=6379
direct_rocksdb_address="192.168.122.48"
direct_rocksdb_port=15001

# Variables for GDPR & Passthrough controller in the CVM
ctl_redis_address="/tmp/redis.sock"
ctl_redis_port=0
ctl_rocksdb_address="/tmp/rocksdb.sock"
ctl_rocksdb_port=0
controller_address="192.168.122.48"
controller_port=1312

config_dir=$(dirname "$(readlink -f "$0")")
images_dir=${config_dir}/../../CVM_setup/images
results_dir_suffix=""

# workload_type="large" # 10M ops
workload_type="medium" # 1M ops
# workload_type="small" # 1K ops

# Default combinations of
#   {1,2,4,8,16,32} clients,
#   {redis, rocksdb} dbs,
#   {workloada workloadb workloadc workloadd workloadf} workloads
dbs="redis rocksdb"
compression_levels="0"
clients="1 2 4 8 16"
workloads="workloada_${workload_type} workloadb_${workload_type} workloadc_${workload_type} workloadd_${workload_type} workloadf_${workload_type}"
# logging setup
logging_compression_levels="0 3 6"
logging_clients="8"
logging_workloads="workloada_monitor_0_${workload_type} workloada_monitor_10_${workload_type} workloada_monitor_50_${workload_type} workloada_monitor_100_${workload_type} \
                    workloadc_monitor_0_${workload_type} workloadc_monitor_10_${workload_type} workloadc_monitor_50_${workload_type} workloadc_monitor_100_${workload_type}"
# GDPR workloads setup
gdpr_clients="8"
gdpr_workloads="workload_customer workload_processor workload_controller"