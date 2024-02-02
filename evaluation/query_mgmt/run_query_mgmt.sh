#!/bin/sh

# Get the directory where the script resides
script_dir="$(dirname "$(readlink -f "$0")")"

# Find the root directory of the repository
project_root=$(git rev-parse --show-toplevel 2>/dev/null)

outputs_folder="$script_dir/query_mgmt_outs"
results_csv_file="query_mgmt_res.csv"
user=$(whoami)
db_dump_and_logs_dir="/scratch/${user}/query_mgmt_data"

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
    controller_path=$project_root/GDPRuler.py
  elif [ $controller == native ]; then
    controller_path=$project_root/native_ctl.py
  fi

  # setup client script path
  client_path=$project_root/client.py

  # clear logs and db dumps
  rm -rf ${db_dump_and_logs_dir}
  mkdir ${db_dump_and_logs_dir}

  if [ $db == rocksdb ]; then
    # Check that the rocksdb_server executable is available
    if [ ! -f "$project_root/controller/build/rocksdb_server" ]; then
      echo "Rocksdb server not found. Please compile the rocksdb server available with the controller."
      exit
    fi
    # run rocksdb server
    $project_root/controller/build/rocksdb_server 15001 ${db_dump_and_logs_dir} > ${outputs_folder}/${test_name_suffix}_server.txt &

    # wait for server to start up
    sleep 3
  elif [ $db == redis ]; then
    # Check that the redis-server executable is available
    if [ ! -f "$project_root/KVs/redis/src/redis-server" ]; then
      echo "Redis server not found. Please compile the redis version of the provided submodule."
      exit
    fi
    # run redis server
    $project_root/KVs/redis/src/redis-server --dir ${db_dump_and_logs_dir} --protected-mode no > ${outputs_folder}/${test_name_suffix}_server.txt &

    # wait for server to start up
    sleep 10
  fi

  # prepare the default data policies of the clients
  for ((i=0; i<$n_clients; i++)); do
    # pur is the total number of purposes (64 for the bitmap)
    # clients is the total number of clients (64 for our setup)
    $script_dir/default_policy_creator.sh -uid $i -pur 64 -clients 64
  done

  # tune controller script args. do not include user policy for native controller.
  ctl_script_args="$controller_path --db $db"
  if [ $controller == gdpr ]; then
    ctl_script_args="$ctl_script_args --config $script_dir/configs/client0_config.json --logpath $db_dump_and_logs_dir"
    # FIX ME!
    # ctl_script_args="$ctl_script_args --config $script_dir/configs/client${i}_config.json --logpath $db_dump_and_logs_dir"
  fi
  # run the controller
  python3 ${ctl_script_args} > ${outputs_folder}/${test_name_suffix}_controller.txt &
  ctl_pid=$!
  sleep 2

  # tune the client script args. We omit address and port as we use the default values for both
  client_script_args="$client_path --workload $project_root/workload_traces/$workload_name --clients $n_clients"
  # run the clients
  python3 ${client_script_args} > ${outputs_folder}/${test_name_suffix}_clients.txt &
  client_pid=$!

  wait ${client_pid}
  status=$?
  if [ $status -ne 0 ]; then
    echo "Client(s) in test $test_name_suffix exited with non-zero status code: $?" >&2
    exit 1
  else
    echo "Client(s) in test $test_name_suffix finished successfully. Output:"
    # Direct client output to stdout
    cat ${outputs_folder}/${test_name_suffix}_clients.txt
    # Retrieve the client results from the temp files
    elapsed_time=$(grep "Elapsed time: " ${outputs_folder}/${test_name_suffix}_clients.txt | awk '{print $3}')
    avg_latency=$(grep "Average Latency: " ${outputs_folder}/${test_name_suffix}_clients.txt | awk '{print $3}')
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
  rm ${outputs_folder}/${test_name_suffix}_server.txt
  rm ${outputs_folder}/${test_name_suffix}_controller.txt
  rm ${outputs_folder}/${test_name_suffix}_clients.txt

  # Write the avg controller and system time to the result csv file
  echo -e "$workload,$controller,$db,$n_clients,$elapsed_time,$avg_latency" >> ${outputs_folder}/${results_csv_file}

}

# build controller
echo "Building controller"
pushd $project_root/controller
cmake -S . -B build -D CMAKE_BUILD_TYPE=Release -D ENCRYPTION_ENABLED=OFF; cmake --build build -j$(nproc)
popd

# create test_outputs folder to store the output txt files
mkdir -p ${outputs_folder}

# create test outputs csv file
if [ ! -f "${outputs_folder}/${results_csv_file}" ]; then
  touch ${outputs_folder}/${results_csv_file}
  echo -e "workload,controller,db,n_clients,elapsed_time (s),avg_latency (s)" >> ${outputs_folder}/${results_csv_file}
fi

# TESTS with combibations of
#   {1,2,4,8,16,32} clients,
#   {redis, rocksdb} dbs,
#   {workloada_test, workloadb_test, workloadc_test, workloadd_test, workloadf_test} workloads
clients="1 2 4 8 16 32"
dbs="redis rocksdb"
workloads="workloada workloadb workloadc workloadd workloadf"
controllers="native gdpr"

for n_clients in $clients; do
  for db in $dbs; do
    for workload in $workloads; do
      for controller in $controllers; do
        echo "Starting experiment with $n_clients clients, $db store, $controller controller, and $workload."
        run_test $n_clients $workload $db $controller
        echo ""
      done
    done
  done
done
