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

# Prepare host storage
echo "Preparing storage for bare metal experiments..."
prepare_storage_host "$NVME_DEVICE" "$MOUNT_POINT" "$FILESYSTEM_TYPE"
# Set cleanup trap
trap 'cleanup_storage_host "$MOUNT_POINT"' EXIT INT TERM

# GDPR controller
results_csv_file=${script_dir}/results${results_dir_suffix}/gdpr_bare_metal-query_mgmt_${workload_type}-encryption_$encryption-logging_$logging-connection_${server_connection}.csv

# prepare the client configs and set the client config file appropriately
max_clients=$(echo $clients | tr ' ' '\n' | sort -nr | head -1)
prepare_configs $max_clients
client_cfg=$script_dir/../configs/

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

controller="gdpr"

for compression_level in $compression_levels_to_use; do
  # recompile the controller with the appropriate compression level
  echo "Building controller with encryption set to $encryption and compression level set to $compression_level"
  pushd $script_dir/../../controller
  rm -rf build
  cmake -S . -B build -D CMAKE_BUILD_TYPE=Release -D DEBUG_FLAG=OFF -D METADATA_CACHE=ON -D CACHE_STATS=OFF -D ASAN_ENABLED=OFF -D ENCRYPTION_ENABLED=$encryption -D LOGGER_COMPRESSION_LEVEL=$compression_level;
  cmake --build build -j$(nproc)
  popd
  for n_clients in $clients_to_use; do
    for db in $dbs; do
      for workload in $workloads_to_use; do
        if [[ $db == "rocksdb" ]]; then
          if [[ $server_connection == "TCP" ]]; then
            db_port=$direct_rocksdb_port
            db_address=$direct_rocksdb_address
          elif [[ $server_connection == "UNIX" ]]; then
            db_port=$ctl_rocksdb_port
            db_address=$ctl_rocksdb_address
          fi
        elif [[ $db == "redis" ]]; then
          if [[ $server_connection == "TCP" ]]; then
            db_port=$direct_redis_port
            db_address=$direct_redis_address
          elif [[ $server_connection == "UNIX" ]]; then
            db_port=$ctl_redis_port
            db_address=$ctl_redis_address
          fi
        fi
        echo -e "\e[34mStarting a native GDPR controller scenario with $n_clients clients, $db store, $controller controller, $workload, logging set to $logging (compression level = $compression_level), and server connection set to $server_connection\e[0m"
        run_experiment native_ctl $n_clients $workload $db $db_address $db_port \
          $results_csv_file $controller $controller_address $controller_port $client_cfg $compression_level $use_drain
        echo ""
      done
    done
  done
done

print_summary
