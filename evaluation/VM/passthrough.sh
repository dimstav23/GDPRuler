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
  echo "Error: server_connection must be 'UNIX' for the passthrough, VM experiment. TCP is currently not supported."
  exit 1
fi

# compile the controller in the CVM with the appropriate encryption option
virt-customize --add ${images_dir}/gdpr.img --smp $(nproc) --memsize 16384 \
  --run-command "cd /root/GDPRuler/controller && rm -rf build && cmake -S . -B build -D CMAKE_BUILD_TYPE=Release -D ENCRYPTION_ENABLED=$encryption && cmake --build build -j$(nproc)"

# GDPR controller
results_csv_file=${script_dir}/results/passthrough_CVM-query_mgmt_${workload_type}-encryption_$encryption-logging_$logging-connection_${server_connection}.csv
for n_clients in $clients; do
  # copy the configs in the controller image
  virt-customize --add ${images_dir}/gdpr.img --copy-in ${script_dir}/../configs:/root
  # set the client config file appropriately
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
      echo "Starting a passthrough CVM scenario run with $n_clients clients, $db store, passthrough controller, $workload, logging set to $logging, and server connection set to $server_connection"
      run_experiment CVM_passthrough $n_clients $workload $db $db_address $db_port \
        $results_csv_file $controller_address $controller_port $client_cfg
      echo ""
    done
  done
done

print_summary