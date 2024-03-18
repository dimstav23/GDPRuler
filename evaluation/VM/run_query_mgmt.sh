#!/bin/sh

script_dir=$(dirname "$(readlink -f "$0")")
images_dir=/scratch/dimitrios/images

# source the common.sh from the evaluation directory
source $script_dir/../common.sh
# source the args_and_checks.sh from the evaluation directory
source $script_dir/../args_and_checks.sh

function prepare_configs() {
  local clients=$1

  # logging is retrieved from cmdline arguments
  if [[ $logging == "ON" ]]; then
    local monitor="true"
  else
    local monitor="false"
  fi
  
  # prepare the default data policies of the clients
  echo "Preparing the client configurations with logging set to ${logging}"
  for ((i=0; i<$clients; i++)); do
    # pur is the total number of purposes (64 for the bitmap)
    # clients is the total number of clients (64 for our setup)
    $script_dir/../default_policy_creator.sh -uid $i -pur 64 -clients 64 -monitor $monitor
  done
}

# Call the parse_args function with your command-line arguments
parse_args_and_checks "$@"

# Variables for the end-to-end test configuration
redis_address="tcp://192.168.122.48"
redis_port=6379
rocksdb_address="192.168.122.48"
rocksdb_port=15001
controller_address="192.168.122.23"
controller_port=1312

# Default combinations of
#   {1,2,4,8,16,32} clients,
#   {redis, rocksdb} dbs,
#   {workloada workloadb workloadc workloadd workloadf} workloads
clients="1 2 4 8 16 32"
dbs="redis rocksdb"
workloads="workloada workloadb workloadc workloadd workloadf"

# compile the controller in the VM with the appropriate encryption option
virt-customize --add ${images_dir}/controller.img --smp $(nproc) --memsize 16384 \
  --run-command "cd /root/GDPRuler/controller && rm -rf build && cmake -S . -B build -D CMAKE_BUILD_TYPE=Release -D ENCRYPTION_ENABLED=$encryption && cmake --build build -j$(nproc)"

# Native controller
results_csv_file=${script_dir}/results/native-query_mgmt-encryption_$encryption-logging_$logging.csv
controller="native"
for n_clients in $clients; do
  for db in $dbs; do
    for workload in $workloads; do
      if [[ $db == "rocksdb" ]]; then
        db_port=$rocksdb_port
        db_address=$rocksdb_address
      elif [[ $db == "redis" ]]; then
        db_port=$redis_port
        db_address=$redis_address
      fi
      echo "Starting a test with $n_clients clients, $db store, $controller controller, and $workload."
      run_VM_test $n_clients $workload $db $db_address $db_port \
      $controller $controller_address $controller_port "" $results_csv_file
      echo ""
    done
  done
done

# GDPR controller
results_csv_file=${script_dir}/results/gdpr-query_mgmt-encryption_$encryption-logging_$logging.csv
controller="gdpr"
for n_clients in $clients; do
  # prepare the client configs
  prepare_configs $n_clients
  # copy the configs in the controller image
  virt-customize --add ${images_dir}/controller.img --copy-in ${script_dir}/../configs:/root
  # set the client config file appropriately
  client_cfg=/root/configs/client0_config.json
  for db in $dbs; do
    for workload in $workloads; do
      if [[ $db == "rocksdb" ]]; then
        db_port=$rocksdb_port
        db_address=$rocksdb_address
      elif [[ $db == "redis" ]]; then
        db_port=$redis_port
        db_address=$redis_address
      fi
      echo "Starting a test with $n_clients clients, $db store, $controller controller, $workload and logging set to $logging"
      run_VM_test $n_clients $workload $db $db_address $db_port \
      $controller $controller_address $controller_port $client_cfg $results_csv_file
      echo ""
    done
  done
done

print_summary
