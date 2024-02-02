#!/bin/sh

test_outputs_folder="test_outputs"
test_results_csv_file="tests.csv"
user=$(whoami)
db_dump_and_logs_dir="/scratch/${user}/test_data"
failed_tests=""

# start a test by running the server and clients
# Args:
#   1: n_clients          (number of clients to run concurrently)
#   2: workload_path      (workload file name expected to be in ./workload_traces/ dir)
#   3: db                 (db to be used in controller. one of {rocksdb, redis})
run_test() {
  local n_clients=$1
  local workload_name=$2
  local db=$3
  local test_name_suffix=${workload_name}_${db}_${n_clients}
  local controller=$4
  local logging=$5
  
  # setup controller path
  if [ $controller == gdpr ]; then
    controller_path=GDPRuler.py
  elif [ $controller == native ]; then
    controller_path=native_ctl.py
  fi

  # setup client script path
  client_path=client.py

  # clear logs and db dumps
  rm -rf ${db_dump_and_logs_dir}
  mkdir ${db_dump_and_logs_dir}

  if [ $db == rocksdb ]; then
    # run rocksdb server
    ./controller/build/rocksdb_server 15001 ${db_dump_and_logs_dir} > ${test_outputs_folder}/${test_name_suffix}_server.txt &

    # wait for server to start up
    sleep 3
  elif [ $db == redis ]; then
    # Check that the redis-server executable is available
    if [ ! -f "./KVs/redis/src/redis-server" ]; then
      echo "Redis server not found. Please compile the redis version of the provided submodule."
      exit
    fi
    # run redis server
    ./KVs/redis/src/redis-server --dir ${db_dump_and_logs_dir} --protected-mode no > ${test_outputs_folder}/${test_name_suffix}_server.txt &

    # wait for server to start up
    sleep 10
  fi

  # tune controller script args. do not include user policy for native controller.
  ctl_script_args="$controller_path --db $db"
  if [ $controller == gdpr ]; then
    if [ $logging == ON ]; then
      ctl_script_args="$ctl_script_args --config ./configs/test_user_logging.json --logpath $db_dump_and_logs_dir"
    else
      ctl_script_args="$ctl_script_args --config ./configs/test_user.json --logpath $db_dump_and_logs_dir"
    fi
  fi
  # run the controller
  python3 ${ctl_script_args} > ${test_outputs_folder}/${test_name_suffix}_controller.txt &
  ctl_pid=$!
  sleep 2

  # tune the client script args. We omit address and port as we use the default values for both
  client_script_args="$client_path --workload ./workload_traces/$workload_name --clients $n_clients"
  # run the clients
  python3 ${client_script_args} > ${test_outputs_folder}/${test_name_suffix}_clients.txt &
  client_pid=$!

  wait ${client_pid}
  status=$?
  if [ $status -ne 0 ]; then
    echo "Client(s) in test $test_name_suffix exited with non-zero status code: $?" >&2
    exit 1
  else
    echo "Client(s) in test $test_name_suffix finished successfully. Output:"
    # Direct client output to stdout
    cat ${test_outputs_folder}/${test_name_suffix}_clients.txt
    # Retrieve the client results from the temp files
    elapsed_time=$(grep "Elapsed time: " ${test_outputs_folder}/${test_name_suffix}_clients.txt | awk '{print $3}')
    avg_latency=$(grep "Average Latency: " ${test_outputs_folder}/${test_name_suffix}_clients.txt | awk '{print $3}')
  fi

  # stopping the controller process
  if [ $controller == native ]; then
    echo "Stopping Native Controller"
    kill $(pgrep -f native_controller)
    sleep 1
  elif [ $controller == gdpr ]; then
    echo "Stopping GDPR Controller"
    kill $(pgrep -f gdpr_controller)
    sleep 1
  fi
  
  # stopping the DB process
  if [ $db == rocksdb ]; then
    # Stop rocksdb server
    kill $(pgrep -f rocksdb_server)
    
    sleep 3
  elif [ $db == redis ]; then
    echo "Stopping redis server"
    # Stop redis server
    kill $(pgrep -f redis-server)

    sleep 10
  fi

  # Remove the temp file for the outputs
  rm ${test_outputs_folder}/${test_name_suffix}_server.txt
  rm ${test_outputs_folder}/${test_name_suffix}_controller.txt
  rm ${test_outputs_folder}/${test_name_suffix}_clients.txt

  if [ -z $avg_latency ]; then
    # Case of a failed test
    failed_tests="$failed_tests $workload,controller=$controller,encr=$encr,logging=$logging,$db,clients=$n_clients"
  else
    # Write the total elapsed time for all the threads and the average latency
    echo -e "$workload,$controller,$encr,$logging,$db,$n_clients,$elapsed_time,$avg_latency" >> ${test_outputs_folder}/${test_results_csv_file}
  fi
}

# Set default encryption value
encr="OFF"
# Check if the script is called with a parameter
if [ $# -gt 0 ]; then
  # Check the parameter value
  case "$1" in
    --encr)
      # Check if the next argument is provided
      if [ $# -gt 1 ]; then
        case "$2" in
          ON)
            encr="ON"
            ;;
          OFF)
            encr="OFF"
            ;;
          *)
            echo "Invalid value for --encr. Please use 'ON' or 'OFF'."
            exit 1
            ;;
        esac
      else
          echo "Value for --encr is missing. Please provide 'ON' or 'OFF'."
          exit 1
      fi
      ;;
    *)
      echo "Invalid option. Usage: $0 --encr [ON|OFF]"
      exit 1
      ;;
  esac
fi

if [ ! -d "controller" ]; then
  echo "controller directory does not exist."
  echo "Please execute the script from the root folder of GDPRuler repository"
  exit
fi

# build controller
echo "Building controller with encryption set to $encr"
cd controller
cmake -S . -B build -D CMAKE_BUILD_TYPE=Release -D ENCRYPTION_ENABLED=$encr;
cmake --build build -j$(nproc)
cd ..

# create test_outputs folder to store the output txt files
mkdir -p ${test_outputs_folder}

# create test outputs csv file
if [ ! -f "${test_outputs_folder}/${test_results_csv_file}" ]; then
  touch ${test_outputs_folder}/${test_results_csv_file}
  echo -e "workload,controller,encryption,logging,db,n_clients,elapsed_time (s),avg_latency (s)" >> ${test_outputs_folder}/${test_results_csv_file}
fi

# TESTS with combibations of
#   {1,2,4,8,16,32} clients,
#   {redis, rocksdb} dbs,
#   {workloada_test, workloadb_test, workloadc_test, workloadd_test, workloadf_test} workloads
clients="1 2 4 8 16 32"
dbs="redis rocksdb"
workloads="workloada_test workloadb_test workloadc_test workloadd_test workloadf_test"

# Native controller
controller="native"
logging_opts="OFF"
for n_clients in $clients; do
  for db in $dbs; do
    for workload in $workloads; do
      echo "Starting a test with $n_clients clients, $db store, $controller controller, and $workload."
      run_test $n_clients $workload $db $controller $logging
      echo ""
    done
  done
done

# GDPR controller
controller="gdpr"
logging_opts="OFF ON"
for n_clients in $clients; do
  for db in $dbs; do
    for workload in $workloads; do
      for logging in $logging_opts; do
        echo "Starting a test with $n_clients clients, $db store, $controller controller, and $workload with logging set to $logging."
        run_test $n_clients $workload $db $controller $logging
        echo ""
      done
    done
  done
done

# Summary of the performed tests
if [ -n "$failed_tests" ]; then
  echo -e "\e[31mThe following tests failed:\e[0m"
  for test in $failed_tests; do
    echo -e "\e[31m$test\e[0m"
  done
else
  echo -e "\e[32mAll tests were successful :)\e[0m"
fi
