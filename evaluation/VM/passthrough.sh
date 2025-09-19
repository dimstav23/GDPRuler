#!/bin/sh

script_dir=$(dirname "$(readlink -f "$0")")

# source the config.sh from the current directory to get the configuration
source $script_dir/config.sh
# source the common.sh from the evaluation directory
source $script_dir/../common.sh
# source the args_and_checks.sh from the evaluation directory
source $script_dir/../args_and_checks.sh

# Call the parse_args function with your command-line arguments
parse_args_and_checks "$@"

if [[ "$server_connection" != "UNIX" ]]; then
  echo "Error: server_connection must be 'UNIX' for the passthrough, CVM experiment. TCP is currently not supported."
  exit 1
fi

# Trap to ensure CVM cleanup on exit
trap "shutdown_cvm" EXIT INT TERM

# Compile the controller in the CVM with the appropriate encryption option
virt-customize --add ${images_dir}/gdpr.img --smp $(nproc) --memsize 16384 \
  --run-command "cd /root/GDPRuler/controller && rm -rf build && cmake -S . -B build -D CMAKE_BUILD_TYPE=Release -D DEBUG_FLAG=OFF -D METADATA_CACHE=ON -D CACHE_STATS=OFF -D ENCRYPTION_ENABLED=$encryption && cmake --build build -j$(nproc)"

# Passthrough controller
results_csv_file=${script_dir}/results${results_dir_suffix}/passthrough_CVM-query_mgmt_${workload_type}-encryption_$encryption-logging_$logging-connection_${server_connection}.csv

# Function to run all experiments in a single CVM
run_experiments_in_cvm() {
  local cvm_pid=""
  
  # Start CVM once
  echo "Starting CVM for all passthrough experiments..."
  boot_cvm
  cvm_pid=$!
   
  # Run all experiments
  for n_clients in $clients; do
    client_cfg="no_cfg"
    for db in $dbs; do
      for workload in $workloads; do
        if [[ $db == "rocksdb" ]]; then
          db_port=$ctl_rocksdb_port
          db_address=$ctl_rocksdb_address
        elif [[ $db == "redis" ]]; then
          db_port=$ctl_redis_port
          db_address=$ctl_redis_address
        fi

        echo -e "\e[34mStarting a passthrough CVM scenario run with $n_clients clients, $db store, passthrough controller, $workload, logging set to $logging, and server connection set to $server_connection\e[0m"

        # Run experiment using existing CVM
        run_experiment CVM_passthrough $n_clients $workload $db $db_address $db_port \
          $results_csv_file $USE_DRAIN $controller_address $controller_port $client_cfg
        echo ""
      done
    done
  done
}

run_experiments_in_cvm
print_summary