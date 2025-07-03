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

# GDPR controller
results_csv_file=${script_dir}/results/gdpr_bare_metal-query_mgmt_${workload_type}-encryption_$encryption-logging_$logging-connection_${server_connection}.csv
controller="gdpr"
for n_clients in $clients; do
  # prepare the client configs
  prepare_configs $n_clients
  # set the client config file appropriately
  client_cfg=$script_dir/../configs/
  for db in $dbs; do
    for workload in $workloads; do
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
      echo "Starting a run with $n_clients clients, $db store, $controller controller, $workload, logging set to $logging, and server connection set to $server_connection"
      run_experiment native_ctl $n_clients $workload $db $db_address $db_port \
        $results_csv_file $controller $controller_address $controller_port $client_cfg
      echo ""
    done
  done
done

print_summary
