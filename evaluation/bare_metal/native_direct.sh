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

# Direct client-server communication natively
results_csv_file=${script_dir}/results/direct-query_mgmt_$number_of_queries-encryption_$encryption-logging_$logging.csv
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
      echo "Starting a run with $n_clients clients, $db store, direct connection, and $workload."
      run_native_direct_experiment $n_clients $workload $db $db_address $db_port $results_csv_file
      echo ""
    done
  done
done

print_summary
