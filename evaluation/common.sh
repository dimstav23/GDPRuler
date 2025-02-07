#!/bin/sh

set -e

# Find the root directory of the repository
project_root=$(git rev-parse --show-toplevel 2>/dev/null)

# Server executables
rocksdb_server_bin="$project_root/controller/build/rocksdb_server"
redis_server_bin="$project_root/KVs/redis/src/redis-server"

# Expect scripts
controller_expect_script="$project_root/evaluation/VM/CVM_GDPRuler.expect"
server_expect_script="$project_root/evaluation/VM/VM_server.expect"

# Directory for storing temporary files for each experiment
tmp_dir=/tmp

# Directory for storing the DB files and the log files
db_dump_and_logs_dir="/scratch/$(whoami)/data"

# List of tests that failed
failed_tests=""

# Universal (C)VM settings
VM_cores="16"
VM_memory="16384"

NODE_BIND="numactl --cpunodebind=0 --membind=0"

# Helper functions for the evaluation and workload execution

# Function to run RocksDB server in a specified environment
# Args:
#   1: server_type   (type of server execution: bare-metal, VM or CVM)
#   2: db_address    (database IP address)
#   3: port          (port for the server)
#   4: log_dir       (directory for logs)
#   5: output_file   (temporary output file)
function run_rocksdb() {
  local server_type="$1"
  local db_address="$2"
  local port="$3"
  local log_dir="$4"
  local output_file="$5"

  # Run the rocksDB server
  if [[ $server_type == "bare-metal" ]]; then

    if [ ! -f $rocksdb_server_bin ]; then
      echo "Rocksdb server not found. Please compile the rocksdb server available with the controller."
      exit
    fi
    echo "Starting rocksdb server"
    # run rocksdb server
    $NODE_BIND $rocksdb_server_bin $port $log_dir > $output_file &
    # wait for the server to be initialized and listen to connections
    wait_for_activation "localhost" $port

  elif [[ $server_type == "VM" ]] || [[ $server_type == "CVM" ]]; then

    echo "Starting rocksdb server in a ${server_type}"
    expect $server_expect_script $server_type "rocksdb" $VM_cores $VM_memory $port $log_dir $output_file &

    # wait for the server to be initialized and listen to connections
    wait_for_activation $db_address $port

  fi
  
}

# Function to run Redis server in a specified environment
# Args:
#   1: server_type   (type of server execution: bare-metal, VM or CVM)
#   2: db_address    (database IP address)
#   3: port          (port for the server)
#   4: log_dir       (directory for logs)
#   5: output_file   (temporary output file)
function run_redis() {
  local server_type="$1"
  local db_address="$2"
  local port="$3"
  local log_dir="$4"
  local output_file="$5"

  # Run the redis server
  if [[ $server_type == "bare-metal" ]]; then

      if [ ! -f $redis_server_bin ]; then
        echo "Redis server not found. Please compile the redis version of the provided submodule."
        exit
      fi
      echo "Starting redis server"
      # run redis server
      $NODE_BIND $redis_server_bin --port $port --dir $log_dir --protected-mode no > $output_file &
      # wait for the server to be initialized and listen to connections
      wait_for_activation "localhost" $port

  elif [[ $server_type == "VM" ]] || [[ $server_type == "CVM" ]]; then

    echo "Starting redis server in a ${server_type}"
    expect $server_expect_script $server_type "redis" $VM_cores $VM_memory $port $log_dir $output_file &

    # wait for the server to be initialized and listen to connections
    # for redis: remove the tcp:// in front of the address
    wait_for_activation "${db_address#tcp://}" $port

  fi
}

# Function to run GDPR controller
# Args:
#   1: controller         (controller executable)
#   2: controller_address (address for the controller)
#   3: controller_port    (port for the controller)
#   4: db                 (database type)
#   5: db_address         (database address and port)
#   6: config             (configuration file)
#   7: log_path           (directory for logs)
#   8: output_file        (temporary output file)
function run_gdpr_controller() {
  local controller="$1"
  local controller_address="$2"
  local controller_port="$3"
  local db="$4"
  local db_address="$5"
  local config="$6"
  local log_path="$7"
  local output_file="$8"

  if [ ! -f $controller ]; then
    echo "Controller not found in $controller. Exiting..."
    exit
  fi
  ctl="$controller --db $db --config $config --logpath $log_path --db_address $db_address \
  --controller_address $controller_address --controller_port $controller_port"

  echo "Starting the GDPR controller"
  $NODE_BIND python3 $ctl > $output_file &
  wait_for_activation "localhost" $controller_port
}

# Function to run GDPR controller in a CVM environment
# Args:
#   1: controller_address (address for the controller)
#   2: controller_port    (port for the controller)
#   3: db                 (database type)
#   4: db_address         (database address and port)
#   5: output_file        (temporary output file)
#   6: config             (configuration file)
#   7: log_path           (directory for logs)

function run_gdpr_controller_CVM() {
  local controller_address="$1"
  local controller_port="$2"
  local db="$3"
  local db_address="$4"
  local output_file="$5"
  local config="$6"
  local log_path="$7"

  echo "Starting the GDPR controller in a CVM"
  echo $(pwd)
  expect $controller_expect_script $VM_cores $VM_memory $db $db_address $controller_address $controller_port $output_file $config $log_path &

  wait_for_activation $controller_address $controller_port
}

# Function to run native controller
# Args:
#   1: controller         (controller executable)
#   2: controller_address (address for the controller)
#   3: controller_port    (port for the controller)
#   4: db                 (database type)
#   5: db_address         (database address and port)
#   6: output_file        (temporary output file)
function run_native_controller() {
  local controller="$1"
  local controller_address="$2"
  local controller_port="$3"
  local db="$4"
  local db_address="$5"
  local output_file="$6"

  if [ ! -f $controller ]; then
    echo "Controller not found in $controller. Exiting..."
    exit
  fi
  ctl="$controller --db $db --db_address $db_address \
  --controller_address $controller_address --controller_port $controller_port"

  echo "Starting the native controller"
  $NODE_BIND python3 $ctl > $output_file &
  wait_for_activation "localhost" $controller_port
}

# Function to run client(s) directly connected to the server
# Args:
#   1: client             (client executable)
#   2: db                 (database type)
#   3: db_address         (database address and port)
#   4: workload           (workload file)
#   5: n_clients          (number of clients)
#   6: output_file        (temporary output file)
function run_direct_client() {
  local client="$1"
  local db="$2"
  local db_address="$3"
  local workload="$4"
  local n_clients="$5"
  local output_file="$6"

  if [ ! -f $client ]; then
    echo "Client not found in $client. Exiting..."
    exit
  fi

  client="$client_path --db $db --db_address $db_address --workload $workload --clients $n_clients"
  echo "Starting the client(s): $client"
  $NODE_BIND python3 ${client} > $output_file
  status=$?
  return $status
}

# Function to run client(s) connected to the controller
# Args:
#   1: client             (client executable)
#   2: workload           (workload file)
#   3: n_clients          (number of clients)
#   4: controller_address (controller IP address)
#   5: controller_port    (controller port)
#   6: output_file        (temporary output file)
function run_client() {
  local client="$1"
  local workload="$2"
  local n_clients="$3"
  local controller_address="$4"
  local controller_port="$5"
  local output_file="$6"

  if [ ! -f $client ]; then
    echo "Client not found in $client. Exiting..."
    exit
  fi

  client="$client --workload $workload --clients $n_clients --address $controller_address --port $controller_port"
  echo "Starting the client(s): $client"
  $NODE_BIND python3 ${client} > $output_file
  status=$?
  return $status
}

# Function to wait for a port activation
# Args:
#   1: IP Address    (IP address)
#   2: Port          (port to wait for)
wait_for_activation() {
  local ip_address="$1"
  local port="$2"
  local max_attempts=60

  for ((attempt=1; attempt<=$max_attempts; attempt++)); do
    if nc -z "$ip_address" "$port" &> /dev/null; then
      return
    else
      sleep 1
    fi
  done

  echo "Timeout: $ip_address:$port did not become active within $max_attempts seconds."
  exit 1
}

# Function to wait for a port shutdown
# Args:
#   1: IP Address    (IP address)
#   2: Port          (port to wait for)
wait_for_shutdown() {
  local ip_address="$1"
  local port="$2"
  local max_attempts=60
  for ((attempt=1; attempt<=$max_attempts; attempt++)); do
    if ! nc -z "$ip_address" "$port" &> /dev/null; then
      return
    else
      sleep 1
    fi
  done

  echo "Timeout: $ip_address:$portdid not become inactive within $max_attempts seconds."
  exit 1
}

# Function to prepare experiment directories and result file
# Args:
#   1: result_file   (result file path)
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

# Function to cleanup after the experiment
# It terminates the server and controller processes,
# waits till their port becomes inactive
# and deletes the files generated by the experiment.
# Args:
#   1: controller_address (controller IP address)
#   2: controller_port    (controller port)
#   3: db                 (database type)
#   4: db_address         (database IP address)
#   5: db_port            (database port)
cleanup() {
  local controller_address="$1"
  local controller_port="$2"
  local db="$3"
  local db_address="$4"
  local db_port="$5"

  # Attempt to stop all relevant processes
  echo "Stopping all relevant processes"
  sudo kill -SIGINT $(pgrep -f qemu) 2>/dev/null || true
  kill $(pgrep -f native_controller) 2>/dev/null || true
  kill $(pgrep -f gdpr_controller) 2>/dev/null || true
  kill $(pgrep -f rocksdb_server) 2>/dev/null || true
  kill $(pgrep -f redis-server) 2>/dev/null || true

  # Wait for ports to become inactive
  echo "Waiting for ports to become inactive"
  wait_for_shutdown "$controller_address" "$controller_port"
  wait_for_shutdown "${db_address#tcp://}" "$db_port"

  # Remove all potentially generated files
  echo "Cleaning up files"
  rm -f "${tmp_dir}"/server.txt "${tmp_dir}"/controller.txt "${tmp_dir}"/clients.txt
  rm -rf "${db_dump_and_logs_dir}"

  # Final check and cleanup for any remaining QEMU processes
  sleep 3  # Buffer time for QEMU cleanup
  if pgrep -f qemu > /dev/null; then
    echo "Forcefully terminating remaining QEMU processes..."
    sudo kill -SIGKILL $(pgrep -f qemu) 2>/dev/null || true
  fi

  # Verify all QEMU processes are terminated
  if pgrep -f qemu > /dev/null; then
    echo "Warning: Some QEMU processes may still be running."
  else
    echo "All cleanup operations completed."
  fi
}

# Function to print summary of test results
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

# Start a test by running the server and the clients natively
# Args:
#   1: n_clients          (number of clients to run concurrently)
#   2: workload           (workload file name)
#   3: db                 (db to be used in controller. one of {rocksdb, redis})
#   4: db_address         (address for the DB)
#   5: db_port            (port for the DB)
#   6: results_csv_file   (result file path -- must exist beforehand)
run_native_direct_experiment() {
  local n_clients="$1"
  local workload="$2"
  local db="$3"
  local db_address="$4"
  local db_port="$5"
  local results_csv_file="${6}"

  local db_address_formatted="${db_address}:${db_port}"

  local controller="direct"

  prepare_experiment $results_csv_file

  # Run the db server
  if [[ $db == "rocksdb" ]]; then
    run_rocksdb "bare-metal" "" $db_port $db_dump_and_logs_dir ${tmp_dir}/server.txt
  elif [[ $db == "redis" ]]; then
    run_redis "bare-metal" "" $db_port $db_dump_and_logs_dir ${tmp_dir}/server.txt
  fi

  # Run the client and gather the results
  client_path="$project_root/scripts/direct_client.py"
  # workload_path=${project_root}/workload_traces/${workload}
  run_direct_client $client_path $db $db_address_formatted $workload $n_clients ${tmp_dir}/clients.txt
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

  cleanup $controller_address $controller_port $db $db_address $db_port

  if [ -z $avg_latency ]; then
    # Case of a failed test
    failed_tests="$failed_tests $workload,controller=$controller,$db,clients=$n_clients"
  else
    # Write the total elapsed time for all the threads and the average latency
    echo -e "$workload,$controller,$db,$n_clients,$elapsed_time,$avg_latency" >> ${results_csv_file}
  fi
}

# Start a test by running the server, the controller and the clients natively
# Args:
#   1: n_clients          (number of clients to run concurrently)
#   2: workload           (workload file name)
#   3: db                 (db to be used in controller. one of {rocksdb, redis})
#   4: db_address         (address for the DB)
#   5: db_port            (port for the DB)
#   6: controller         (controller type. one of {gdpr, native})
#   7: controller_address (address of the controller)
#   8: controller_port    (port of the controller)
#   9: config             (configuration file for the client)
#  10: results_csv_file   (result file path -- must exist beforehand)
run_native_ctl_experiment() {
  local n_clients="$1"
  local workload="$2"
  local db="$3"
  local db_address="$4"
  local db_port="$5"
  local controller="$6"
  local controller_address="$7"
  local controller_port="$8"
  local config="$9"
  local results_csv_file="${10}"

  local db_address_formatted="${db_address}:${db_port}"

  prepare_experiment $results_csv_file

  # Run the db server
  if [[ $db == "rocksdb" ]]; then
    run_rocksdb "bare-metal" "" $db_port $db_dump_and_logs_dir ${tmp_dir}/server.txt
  elif [[ $db == "redis" ]]; then
    run_redis "bare-metal" "" $db_port $db_dump_and_logs_dir ${tmp_dir}/server.txt
  fi

  # Run the controller
  if [[ $controller == "gdpr" ]]; then
    controller_path="$project_root/scripts/GDPRuler.py"
    run_gdpr_controller $controller_path $controller_address $controller_port \
    $db $db_address_formatted $config $db_dump_and_logs_dir ${tmp_dir}/controller.txt
  elif [[ $controller == "native" ]]; then
    controller_path="$project_root/scripts/native_ctl.py"
    run_native_controller $controller_path $controller_address $controller_port \
    $db $db_address_formatted ${tmp_dir}/controller.txt
  fi

  # Run the client and gather the results
  client_path="$project_root/scripts/client.py"
  # workload_path=${project_root}/workload_traces/${workload}
  run_client $client_path $workload $n_clients $controller_address $controller_port ${tmp_dir}/clients.txt
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

  cleanup $controller_address $controller_port $db $db_address $db_port

  if [ -z $avg_latency ]; then
    # Case of a failed test
    failed_tests="$failed_tests $workload,controller=$controller,$db,clients=$n_clients"
  else
    # Write the total elapsed time for all the threads and the average latency
    echo -e "$workload,$controller,$db,$n_clients,$elapsed_time,$avg_latency" >> ${results_csv_file}
  fi
}

# Start a test by running the server in a bare-metal or within a (C)VM, 
# the controller in a CVM and the clients natively
# Args:
#   1: server_type        (type of the server deployment)
#   2: n_clients          (number of clients to run concurrently)
#   3: workload           (workload file name)
#   4: db                 (db to be used in controller. one of {rocksdb, redis})
#   5: db_address         (address for the DB)
#   6: db_port            (port for the DB)
#   7: controller_address (address of the controller)
#   8: controller_port    (port of the controller)
#   9: config             (configuration file for the client)
#  10: results_csv_file   (result file path -- must exist beforehand)
run_VM_ctl_experiment() {
  local server_type="$1"
  local n_clients="$2"
  local workload="$3"
  local db="$4"
  local db_address="$5"
  local db_port="$6"
  local controller_address="$7"
  local controller_port="$8"
  local config="${9}"
  local results_csv_file="${10}"

  local db_address_formatted="${db_address}:${db_port}"

  local controller="gdpr"

  prepare_experiment $results_csv_file

  # Run the db server
  if [[ $db == "rocksdb" ]]; then
    run_rocksdb $server_type $db_address $db_port $db_dump_and_logs_dir ${tmp_dir}/server.txt
  elif [[ $db == "redis" ]]; then
    run_redis $server_type $db_address $db_port $db_dump_and_logs_dir ${tmp_dir}/server.txt
  fi

  # Run the GDPR controller in a CVM
  run_gdpr_controller_CVM $controller_address $controller_port \
    $db $db_address_formatted ${tmp_dir}/controller.txt $config $db_dump_and_logs_dir

  # Run the client and gather the results
  client_path="$project_root/scripts/client.py"
  # workload_path=${project_root}/workload_traces/${workload}
  run_client $client_path $workload $n_clients $controller_address $controller_port ${tmp_dir}/clients.txt
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

  cleanup $controller_address $controller_port $db $db_address $db_port

  if [ -z $avg_latency ]; then
    # Case of a failed test
    failed_tests="$failed_tests $workload,controller=$controller,$db,clients=$n_clients"
  else
    # Write the total elapsed time for all the threads and the average latency
    echo -e "$workload,$controller,$db,$n_clients,$elapsed_time,$avg_latency" >> ${results_csv_file}
  fi
}