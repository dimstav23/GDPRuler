#!/bin/sh

set -e

# Get the directory where the script resides
script_dir="$(dirname "$(readlink -f "$0")")"

# Find the root directory of the repository
project_root=$(git rev-parse --show-toplevel 2>/dev/null)

# Server executables
rocksdb_server_bin="$project_root/controller/build/rocksdb_server"
redis_server_bin=$project_root/KVs/redis/src/redis-server

# Directory for storing temporary files for each experiment
tmp_dir=/tmp

# Directory for storing the DB files and the log files
db_dump_and_logs_dir="/scratch/$(whoami)/data"

# List of tests that failed
failed_tests=""

# Helper functions for the evaluation and workload execution

function run_rocksdb() {
  local port="$1"
  local log_dir="$2"
  local output_file="$3"

  if [ ! -f $rocksdb_server_bin ]; then
    echo "Rocksdb server not found. Please compile the rocksdb server available with the controller."
    exit
  fi
  echo "Starting rocksdb server"
  # run rocksdb server
  $rocksdb_server_bin $port $log_dir > $output_file &
  # wait for the server to be initialized and listen to connections
  wait_for_activation $port
}

function run_redis() {
  local port="$1"
  local log_dir="$2"
  local output_file="$3"

  if [ ! -f $redis_server_bin ]; then
    echo "Redis server not found. Please compile the redis version of the provided submodule."
    exit
  fi
  echo "Starting redis server"
  # run redis server
  $redis_server_bin --dir $log_dir --protected-mode no > $output_file &
  # wait for the controller to be initialized and listen to connections
  wait_for_activation $port
}

function run_gdpr_controller() {
  local controller="$1"
  local controller_port="$2"
  local db="$3"
  local config="$4"
  local log_path="$5"
  local output_file="$6"

  if [ ! -f $controller ]; then
    echo "Controller not found in $controller. Exiting..."
    exit
  fi
  ctl="$controller --db $db --config $config --logpath $log_path"

  echo "Starting the GDPR controller"
  python3 $ctl > $output_file &
  wait_for_activation $controller_port
}

function run_native_controller() {
  local controller="$1"
  local controller_port="$2"
  local db="$3"
  local output_file="$4"

  if [ ! -f $controller ]; then
    echo "Controller not found in $controller. Exiting..."
    exit
  fi
  ctl="$controller --db $db"

  echo "Starting the native controller"
  python3 $ctl > $output_file &
  wait_for_activation $controller_port
}

function run_client() {
  local client="$1"
  local workload="$2"
  local n_clients="$3"
  local output_file="$4"

  if [ ! -f $client ]; then
    echo "Client not found in $client. Exiting..."
    exit
  fi

  if [ ! -f $workload ]; then
    echo "$workload not found. Exiting..."
    exit
  fi
  
  client="$client_path --workload $workload --clients $n_clients"
  echo "Starting the client(s)"
  python3 ${client} > $output_file
  status=$?
  return $status
}

wait_for_activation() {
  local port="$1"
  local max_attempts=30

  for ((attempt=1; attempt<=$max_attempts; attempt++)); do
    if lsof -i :"$port" &> /dev/null; then
      return
    else
      sleep 1
    fi
  done

  echo "Timeout: $port did not become active within $max_attempts seconds."
  exit 1
}

wait_for_shutdown() {
  local port="$1"
  local max_attempts=30

  for ((attempt=1; attempt<=$max_attempts; attempt++)); do
    if ! lsof -i :"$port" &> /dev/null; then
      return
    else
      sleep 1
    fi
  done

  echo "Timeout: $port did not become inactive within $max_attempts seconds."
  exit 1
}

prepare_experiment() {
  local result_file="$1"

  # Make sure that the tmp directory is created
  mkdir -p $tmp_dir
  # Delete and recreate the folder for the db files and logs
  rm -rf $db_dump_and_logs_dir
  mkdir -p $db_dump_and_logs_dir

  # Create the result file, if it doesn't exist
  # and add its first line with the csv columns
  if [ ! -f "${result_file}" ]; then
    install -D -m 644 /dev/null ${result_file}
    echo -e "workload,controller,db,n_clients,elapsed_time (s),avg_latency (s)" >> ${result_file}
  fi
}

cleanup() {
  local controller="$1"
  local controller_port="$2"
  local db="$3"
  local db_port="$4"

  # stopping the controller process
  if [ $controller == native ]; then
    echo "Stopping Native Controller"
    kill $(pgrep -f native_controller)
  elif [ $controller == gdpr ]; then
    echo "Stopping GDPR Controller"
    kill $(pgrep -f gdpr_controller)
  fi
  wait_for_shutdown $controller_port

  # stopping the DB process
  if [ $db == rocksdb ]; then
    echo "Stopping rocksdb server"
    kill $(pgrep -f rocksdb_server)
  elif [ $db == redis ]; then
    echo "Stopping redis server"
    kill $(pgrep -f redis-server)
  fi
  wait_for_shutdown $db_port

  # Remove the temp file and the generated db and logs files
  rm ${tmp_dir}/server.txt
  rm ${tmp_dir}/controller.txt
  rm ${tmp_dir}/clients.txt
  rm -rf ${db_dump_and_logs_dir}
}

print_summary() {
  # Summary of the performed tests
  if [ -n "$failed_tests" ]; then
    echo -e "\e[31mThe following tests failed:\e[0m"
    for test in $failed_tests; do
      echo -e "\e[31m$test\e[0m"
    done
  else
    echo -e "\e[32mAll tests were successful :)\e[0m"
  fi
}

# Start a test by running the server, the controller and the clients
# Args:
#   1: n_clients          (number of clients to run concurrently)
#   2: workload           (workload file name)
#   3: db                 (db to be used in controller. one of {rocksdb, redis})
#   4: db_port            (port for the DB)
#   5: controller         (controller type. one of {gdpr, native})
#   6: controller_port    (port of the controller)
#   7: config             (configuration file for the client)
#   8: results_csv_file   (result file path -- must exist beforehand)
run_native_test() {
  local n_clients=$1
  local workload=$2
  local db=$3
  local db_port=$4
  local controller=$5
  local controller_port=$6
  local config=$7
  local results_csv_file=$8

  prepare_experiment $results_csv_file  

  # Run the db server
  if [ $db == rocksdb ]; then
    run_rocksdb $db_port $db_dump_and_logs_dir ${tmp_dir}/server.txt
  elif [ $db == redis ]; then
    run_redis $db_port $db_dump_and_logs_dir ${tmp_dir}/server.txt
  fi

  # Run the controller
  if [ $controller == gdpr ]; then
    controller_path="$project_root/GDPRuler.py"
    run_gdpr_controller $controller_path $controller_port $db $config $db_dump_and_logs_dir ${tmp_dir}/controller.txt
  elif [ $controller == native ]; then
    controller_path="$project_root/native_ctl.py"
    run_native_controller $controller_path $controller_port $db ${tmp_dir}/controller.txt
  fi

  # Run the client and gather the results
  client_path="$project_root/client.py"
  workload_path=${project_root}/workload_traces/${workload}
  run_client $client_path $workload_path $n_clients ${tmp_dir}/clients.txt
  status=$?
  if [ $status -ne 0 ]; then
    echo "Client(s) with the following config \"${workload},${db},${controller},${n_clients}\" exited with non-zero status code: $?" >&2
    exit 1
  else
    echo "Client(s) with the following config \"${workload},${db},${controller},${n_clients}\" finished successfully. Output:"
    # Direct client output to stdout for better observability
    cat ${tmp_dir}/clients.txt
    # Retrieve the client results from the temp files
    elapsed_time=$(grep "Elapsed time: " ${tmp_dir}/clients.txt | awk '{print $3}')
    avg_latency=$(grep "Average Latency: " ${tmp_dir}/clients.txt | awk '{print $3}')
  fi

  cleanup $controller $controller_port $db $db_port

  if [ -z $avg_latency ]; then
    # Case of a failed test
    failed_tests="$failed_tests $workload,controller=$controller,$db,clients=$n_clients"
  else
    # Write the total elapsed time for all the threads and the average latency
    echo -e "$workload,$controller,$db,$n_clients,$elapsed_time,$avg_latency" >> ${results_csv_file}
  fi
}