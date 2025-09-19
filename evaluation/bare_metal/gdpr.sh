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
  USE_DRAIN="true"
else
  workloads_to_use=$workloads
fi

controller="gdpr"
for n_clients in $clients; do
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
      echo -e "\e[34mStarting a native GDPR controller scenario with $n_clients clients, $db store, $controller controller, $workload, logging set to $logging, and server connection set to $server_connection\e[0m"
      run_experiment native_ctl $n_clients $workload $db $db_address $db_port \
        $results_csv_file $USE_DRAIN $controller $controller_address $controller_port $client_cfg
      echo ""
    done
  done
done

print_summary
