#!/bin/sh

test_outputs_folder="test_outputs"
test_results_csv_file="tests.csv"
user=$(whoami)
db_dump_and_logs_dir="/scratch/${user}/test_data"

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

  local controller_times=0
  local system_times=0

  # tune controller script args. do not include user policy for native controller.
  ctl_script_args="$controller_path --db $db"
  if [ $controller == gdpr ]; then
    ctl_script_args="$ctl_script_args --config ./configs/owner_policy.json --logpath $db_dump_and_logs_dir"
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

  # Remove the temp file for server output
  rm ${test_outputs_folder}/${test_name_suffix}_server.txt
  rm ${test_outputs_folder}/${test_name_suffix}_controller.txt
  rm ${test_outputs_folder}/${test_name_suffix}_clients.txt

  # Write the total elapsed time for all the threads and the average latency
  echo -e "$workload,$controller,$db,$n_clients,$elapsed_time,$avg_latency" >> ${test_outputs_folder}/${test_results_csv_file}
}

if [ ! -d "controller" ]; then
  echo "controller directory does not exist."
  echo "Please execute the script from the root folder of GDPRuler repository"
  exit
fi

# build controller
echo "Building controller"
cd controller
cmake -S . -B build; cmake --build build
cd ..

# create test_outputs folder to store the output txt files
mkdir -p ${test_outputs_folder}

# create test outputs csv file
if [ ! -f "${test_outputs_folder}/${test_results_csv_file}" ]; then
  touch ${test_outputs_folder}/${test_results_csv_file}
  echo -e "workload,controller,db,n_clients,elapsed_time (s),avg_latency (s)" >> ${test_outputs_folder}/${test_results_csv_file}
fi

# TESTS with combibations of
#   {1,2,4,8,16,32} clients,
#   {redis, rocksdb} dbs,
#   {workloada_test, workloadb_test, workloadc_test, workloadd_test, workloadf_test} workloads
# c 
clients="1 2"
dbs="redis rocksdb"
#workloads="workload_monitor_vanilla workload_monitor_0 workload_monitor_10 workload_monitor_20 workload_monitor_50 workload_monitor_100"
workloads="workloada_test" # workloadb_test workloadc_test workloadd_test workloadf_test"
controllers="native gdpr"

for n_clients in $clients; do
  for db in $dbs; do
    for workload in $workloads; do
      for controller in $controllers; do
        echo "Starting a test with $n_clients clients, $db store, $controller controller, and $workload."
        run_test $n_clients $workload $db $controller
        echo ""
      done
    done
  done
done
