#!/bin/sh

script_dir=$(dirname "$(readlink -f "$0")")

# source the config.sh from the current directory to get the configuration
source $script_dir/config.sh
# source the common.sh from the evaluation directory
source $script_dir/../common.sh
# source the args_and_checks.sh from the evaluation directory
source $script_dir/../args_and_checks.sh

function prepare_configs() {
  local clients=$1

  # prepare the default data policies of the clients
  echo "Preparing the client configurations with logging set to ${logging}"
  for ((i=0; i<$clients; i++)); do
    # pur is the total number of purposes (64 for the bitmap)
    # clients is the total number of clients (64 for our setup)
    $script_dir/../default_policy_creator.sh -uid $i -pur 64 -clients 64 -monitor "false"
  done
}

# Call the parse_args function with your command-line arguments
parse_args_and_checks "$@"

if [[ "$server_connection" != "UNIX" ]]; then
  echo "Error: server_connection must be 'UNIX' for the GDPR, CVM experiment. TCP is currently not supported."
  exit 1
fi

# Set cleanup trap for CVM-only cleanup
trap 'shutdown_cvm' EXIT INT TERM

# GDPR controller
results_csv_file=${script_dir}/results${results_dir_suffix}/gdpr_CVM-query_mgmt_${workload_type}-encryption_$encryption-logging_$logging-connection_${server_connection}.csv

# Function to run all experiments in a single CVM
run_experiments_in_cvm() {
  local cvm_pid=""
  
  # prepare the client configs and set the client config file appropriately
  max_clients=$(echo $clients | tr ' ' '\n' | sort -nr | head -1)
  prepare_configs $max_clients
  client_cfg=$script_dir/../configs/
  # copy the configs in the controller image
  virt-customize --add ${images_dir}/gdpr.img --copy-in ${script_dir}/../configs:/root

  if [[ $logging == "ON" ]]; then
    workloads_to_use=$logging_workloads
    clients_to_use=$logging_clients
    compression_levels_to_use=$logging_compression_levels
    use_drain="true"
  else
    workloads_to_use=$workloads
    clients_to_use=$clients
    compression_levels_to_use=$compression_levels
    use_drain="false"
  fi

  # Start CVM once
  echo "Starting CVM for all GDPR experiments..."
  boot_cvm
  cvm_pid=$!
  
  # Run all experiments
  for compression_level in $compression_levels_to_use; do
    # recompile the controller with the appropriate compression level and encryption parameter
    cmd="cd /root/GDPRuler/controller && rm -rf build \
     && cmake -S . -B build -D CMAKE_BUILD_TYPE=Release -D DEBUG_FLAG=OFF -D METADATA_CACHE=ON -D CACHE_STATS=OFF -D ENCRYPTION_ENABLED=$encryption -D LOGGER_COMPRESSION_LEVEL=$compression_level -D ENABLE_GDPR_INDEX=OFF \
     && cmake --build build -j$(nproc)"
    execute_in_cvm "$cmd"

    for n_clients in $clients_to_use; do
      for db in $dbs; do
        for workload in $workloads_to_use; do
          if [[ $db == "rocksdb" ]]; then
            db_port=$ctl_rocksdb_port
            db_address=$ctl_rocksdb_address
          elif [[ $db == "redis" ]]; then
            db_port=$ctl_redis_port
            db_address=$ctl_redis_address
          fi

          echo -e "\e[34mStarting a gdpr CVM scenario run with $n_clients clients, $db store, gdpr controller, $workload, logging set to $logging (compression level = $compression_level), and server connection set to $server_connection\e[0m"

          # Run experiment using existing CVM
          run_experiment CVM_gdpr $n_clients $workload $db $db_address $db_port \
            $results_csv_file $controller_address $controller_port $client_cfg $compression_level $use_drain
          echo ""
        done
      done
    done
  done
}

run_experiments_in_cvm
print_summary