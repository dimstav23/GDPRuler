#!/bin/sh

test_outputs_folder="test_outputs"

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

  if [ $db == rocksdb ]; then
    # run rocksdb server
    ./controller/build/rocksdb_server 15001 ./db > ${test_outputs_folder}/${test_name_suffix}_server.txt &

    # wait for server to start up
    sleep 3
  fi

  local controller_times=0
  local system_times=0

  # start clients in parallel and redirect their outputs to different files
  for ((i=1; i<=$n_clients; i++)); do
    python GDPRuler.py --config ./configs/owner_policy.json --workload ./workload_traces/${workload_name} --db ${db} > ${test_outputs_folder}/${test_name_suffix}_client_${i}.txt &
    pids[${i}]=$!
  done


  # Wait for the clients to finish
  for ((i=1; i<=$n_clients; i++)); do
    # echo "Waiting for client $i with pid: ${pids[$i]} to finish"
    wait ${pids[$i]}
    status=$?
    if [ $status -ne 0 ]; then
      echo "Client $i in test $test_name_suffix exited with non-zero status code: $?" >&2
      exit 1
    else
      echo "Client $i in test $test_name_suffix finished successfully. Output:"
      # Direct client output to stdout
      cat ${test_outputs_folder}/${test_name_suffix}_client_${i}.txt
      # Retrieve the client results from the temp files
      controller_time=$(grep "Controller time:" ${test_outputs_folder}/${test_name_suffix}_client_${i}.txt | awk '{print $3}')
      system_time=$(grep "System time:" ${test_outputs_folder}/${test_name_suffix}_client_${i}.txt | awk '{print $3}')
      controller_times=$(echo "$controller_times + $controller_time" | bc -l)
      system_times=$(echo "$system_times + $system_time" | bc -l)
      # Remove the temp file for client output
      rm ${test_outputs_folder}/${test_name_suffix}_client_${i}.txt
    fi
  done

  if [ $db == rocksdb ]; then
    # Stop rocksdb server
    kill $(pgrep rocksdb_server)
    # Remove the temp file for server output
    rm ${test_outputs_folder}/${test_name_suffix}_server.txt
    sleep 3
  fi

  # Calculate the average controller time & system time per client
  avg_controller_time=$(echo "$controller_times / $n_clients" | bc -l | awk '{printf "%.9f", $0}')
  avg_system_time=$(echo "$system_times / $n_clients" | bc -l | awk '{printf "%.9f", $0}')
  if [ $(echo "$avg_controller_time < 0" | bc) -eq 1 ]; then
    avg_controller_time="0$avg_controller_time"
  fi
  if [ $(echo "$avg_system_time < 0" | bc) -eq 1 ]; then
    avg_system_time="0$avg_system_time"
  fi
  # Write the avg controller and system time to the result file
  echo "$workload,controller_time,$avg_controller_time" > ${test_outputs_folder}/${test_name_suffix}.csv
  echo "$workload,system_time,$avg_system_time" > ${test_outputs_folder}/${test_name_suffix}.csv

}

# build controller
echo "Building controller"
cd controller
cmake -S . -B build; cmake --build build
cd ..

# create test_outputs folder to store the output txt files
mkdir -p ${test_outputs_folder}

# TESTS with combibations of
#   {1,2,4,8,16,32} clients,
#   {redis, rocksdb} dbs,
#   {workloada_test, workloadb_test, workloadc_test, workloadd_test, workloadf_test} workloads
clients="1 2 4 8 16 32"
dbs="rocksdb"
workloads="workloada_test workloadb_test workloadc_test workloadd_test workloadf_test"

for n_clients in $clients; do
  for db in $dbs; do
    for workload in $workloads; do
      echo "Starting a test with $n_clients clients, $db store, and $workload."
      run_test $n_clients $workload $db
      echo ""
    done
  done
done
