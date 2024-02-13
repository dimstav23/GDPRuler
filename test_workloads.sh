#!/bin/sh

script_dir=$(dirname "$(readlink -f "$0")")

# source the common.sh from the evaluation
source $script_dir/evaluation/common.sh
# source the args_and_checks.sh from the evaluation
source $script_dir/evaluation/args_and_checks.sh

# Call the parse_args function with your command-line arguments
parse_args_and_checks "$@"

# Variables for the end-to-end test configuration
redis_port=6379
rocksdb_port=15001
controller_port=1312
test_cfg=${script_dir}/configs/test_user.json
test_log_cfg=${script_dir}/configs/test_user_logging.json

# Default tests with combibations of
#   {1,2,4,8,16,32} clients,
#   {redis, rocksdb} dbs,
#   {workloada_test, workloadb_test, workloadc_test, workloadd_test, workloadf_test} workloads
clients="1 2 4 8 16 32"
dbs="redis rocksdb"
workloads="workloada_test workloadb_test workloadc_test workloadd_test workloadf_test"

# Native controller
results_csv_file=${script_dir}/test_outputs/native-tests.csv
controller="native"
for n_clients in $clients; do
  for db in $dbs; do
    for workload in $workloads; do
      if [ $db == rocksdb ]; then
        db_port=$rocksdb_port
      elif [ $db == redis ]; then
        db_port=$redis_port
      fi
      echo "Starting a test with $n_clients clients, $db store, $controller controller, and $workload."
      run_native_test $n_clients $workload $db $db_port $controller $controller_port "" $results_csv_file
      echo ""
    done
  done
done

# GDPR controller w/o logging
results_csv_file=${script_dir}/test_outputs/gdpr-tests.csv
controller="gdpr"
for n_clients in $clients; do
  for db in $dbs; do
    for workload in $workloads; do
      if [ $db == rocksdb ]; then
        db_port=$rocksdb_port
      elif [ $db == redis ]; then
        db_port=$redis_port
      fi
      echo "Starting a test with $n_clients clients, $db store, $controller controller, and $workload"
      run_native_test $n_clients $workload $db $db_port $controller $controller_port $test_cfg $results_csv_file
      echo ""
    done
  done
done

# GDPR controller w/ logging
results_csv_file=${script_dir}/test_outputs/gdpr-log-tests.csv
controller="gdpr"
for n_clients in $clients; do
  for db in $dbs; do
    for workload in $workloads; do
      if [ $db == rocksdb ]; then
        db_port=$rocksdb_port
      elif [ $db == redis ]; then
        db_port=$redis_port
      fi
      echo "Starting a test with $n_clients clients, $db store, $controller controller, and $workload with logging enabled."
      run_native_test $n_clients $workload $db $db_port $controller $controller_port $test_log_cfg $results_csv_file
      echo ""
    done
  done
done

print_summary
