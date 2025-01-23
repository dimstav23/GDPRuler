# Variables for the end-to-end test configuration
redis_address="tcp://127.0.0.1"
redis_port=6379
rocksdb_address="127.0.0.1"
rocksdb_port=15001
controller_address="127.0.0.1"
controller_port=1312

# Default combinations of
#   {1,2,4,8,16,32} clients,
#   {redis, rocksdb} dbs,
#   {workloada workloadb workloadc workloadd workloadf} workloads
clients="1 2 4 8 16"
dbs="redis rocksdb"
workloads="workloada workloadb workloadc workloadd workloadf"
