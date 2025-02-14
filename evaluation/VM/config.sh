# Variables for the end-to-end test configuration
redis_address="tcp://192.168.122.48"
redis_port=6379
rocksdb_address="192.168.122.48"
rocksdb_port=15001
controller_address="192.168.122.23"
controller_port=1312

bare_metal_redis_address="tcp://192.168.122.1"
bare_metal_rocksdb_address="192.168.122.1"

config_dir=$(dirname "$(readlink -f "$0")")
images_dir=${config_dir}/../../CVM_setup/images

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
