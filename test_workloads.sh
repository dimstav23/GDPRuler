#!/bin/sh

test_outputs_folder="test_outputs"

# start a test by running the server and clients
# Args:
#   1: n_server_threads   (number of threads to run the server on)
#   2: n_clients          (number of clients to run concurrently)
#   3: workload_path      (workload file name expected to be in ./workload_traces/ dir)
#   4: db                 (db to be used in controller. one of {rocksdb, redis})
run_test() {
  local n_server_threads=$1
  local n_clients=$2
  local workload_name=$3
  local db=$4
  local test_name_suffix=${workload_name}_${db}_${n_server_threads}_${n_clients}

  if [ $db == rocksdb ]; then
    # run rocksdb server
    ./controller/build/rocksdb_server 15001 ./db $n_server_threads > ${test_outputs_folder}/${test_name_suffix}_server.txt &

    # wait for server to start up
    sleep 3
  fi

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
      cat ${test_outputs_folder}/${test_name_suffix}_client_${i}.txt
    fi
  done

  if [ $db == rocksdb ]; then
    # Stop rocksdb server
    kill $(pgrep rocksdb_server)
    sleep 3
  fi
}

# build controller
echo "Building controller"
cd controller
cmake -S . -B build; cmake --build build
cd ..

# create test_outputs folder to store the output txt files
mkdir -p ${test_outputs_folder}

# TESTS with combibations of
#   {1,2,4,8,16,32} threads,
#   {1,2,4,8,16,32} clients,
#   {redis, rocksdb} dbs,
#   {workloada_test, workloadb_test} workloads
for ((n_server_threads=1; n_server_threads<=32; n_server_threads=n_server_threads*2)); do
  for ((n_clients=1; n_clients<=32; n_clients=n_clients*2)); do
    for db in rocksdb redis; do
      for workload in workloada_test workloadb_test; do
        echo "Starting a test with $n_server_threads threads, $n_clients clients, $db store, and $workload."
        run_test $n_server_threads $n_clients $workload $db
        echo ""
      done
    done
  done
done
