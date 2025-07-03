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

# Native controller
results_csv_file=${script_dir}/results/passthrough_bare_metal-query_mgmt_${workload_type}-encryption_$encryption-logging_$logging-connection_${server_connection}.csv
controller="passthrough"
for n_clients in $clients; do
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
      echo "Starting a run with $n_clients clients, $db store, $controller controller, $workload, and server connection set to $server_connection"
      run_native_ctl_experiment $n_clients $workload $db $db_address $db_port \
      $controller $controller_address $controller_port "no_cfg" $results_csv_file
      echo ""
    done
  done
done

print_summary
