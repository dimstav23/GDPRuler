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

# compile the controller in the CVM with the appropriate encryption option
virt-customize --add ${images_dir}/controller.img --smp $(nproc) --memsize 16384 \
  --run-command "cd /root/GDPRuler/controller && rm -rf build && cmake -S . -B build -D CMAKE_BUILD_TYPE=Release -D ENCRYPTION_ENABLED=$encryption && cmake --build build -j$(nproc)"

# GDPR controller
results_csv_file=${script_dir}/results/gdpr_self_hosted-query_mgmt_${workload_type}-encryption_$encryption-logging_$logging.csv
server_type="bare-metal"
for n_clients in $clients; do
  # prepare the client configs
  prepare_configs $n_clients
  # copy the configs in the controller image
  virt-customize --add ${images_dir}/controller.img --copy-in ${script_dir}/../configs:/root
  # set the client config file appropriately
  client_cfg=/root/configs/
  for db in $dbs; do
    for workload in $workloads; do
      if [[ $db == "rocksdb" ]]; then
        db_port=$rocksdb_port
        db_address=$bare_metal_rocksdb_address
      elif [[ $db == "redis" ]]; then
        db_port=$redis_port
        db_address=$bare_metal_redis_address
      fi
      echo "Starting a self hosted scenario run with $n_clients clients, $db store, GDPR controller, $workload and logging set to $logging"
      run_VM_ctl_experiment $server_type $n_clients $workload $db $db_address $db_port \
      $controller_address $controller_port $client_cfg $results_csv_file
      echo ""
    done
  done
done

print_summary