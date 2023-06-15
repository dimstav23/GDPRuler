#!/bin/sh

# Get the directory where the script resides
script_dir="$(dirname "$(readlink -f "$0")")"

# Find the root directory of the repository
project_root=$(git rev-parse --show-toplevel 2>/dev/null)

outputs_folder="$script_dir/logging_encr_outs"
results_csv_file="logging_encr_res.csv"
user=$(whoami)
db_dump_and_logs_dir="/scratch/${user}/logging_encr_data"

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

  local controller_times=0
  local system_times=0

  # prepare the default data policies of the clients
  for ((i=0; i<$n_clients; i++)); do
    # pur is the total number of purposes (64 for the bitmap)
    # clients is the total number of clients (64 for our setup)
    $script_dir/default_policy_creator.sh -uid $i -pur 64 -clients 64
  done

  # start clients in parallel and redirect their outputs to different files
  for ((i=0; i<$n_clients; i++)); do
    # tune script args. do not include user policy for native controller.
    script_args="$controller_path --workload $project_root/workload_traces/$workload_name --db $db"
    if [ $controller == gdpr ]; then
      script_args="$script_args --config $script_dir/configs/client${i}_config.json --logpath $db_dump_and_logs_dir"
    fi
    
    # run the workload
    python ${script_args} > ${outputs_folder}/${test_name_suffix}_client_${i}.txt &
    pids[${i}]=$!
  done


  # Wait for the clients to finish
  for ((i=0; i<$n_clients; i++)); do
    # echo "Waiting for client $i with pid: ${pids[$i]} to finish"
    wait ${pids[$i]}
    status=$?
    if [ $status -ne 0 ]; then
      echo "Client $i in test $test_name_suffix exited with non-zero status code: $?" >&2
      exit 1
    else
      echo "Client $i in test $test_name_suffix finished successfully. Output:"
      # Direct client output to stdout
      cat ${outputs_folder}/${test_name_suffix}_client_${i}.txt
      # Retrieve the client results from the temp files
      controller_time=$(grep "Controller time:" ${outputs_folder}/${test_name_suffix}_client_${i}.txt | awk '{print $3}')
      system_time=$(grep "System time:" ${outputs_folder}/${test_name_suffix}_client_${i}.txt | awk '{print $3}')
      controller_times=$(echo "$controller_times + $controller_time" | bc -l)
      system_times=$(echo "$system_times + $system_time" | bc -l)
      # Remove the temp file for client output
      rm ${outputs_folder}/${test_name_suffix}_client_${i}.txt
    fi
  done

  if [ $db == rocksdb ]; then
    echo "Stopping rocksdb server"
    # Stop rocksdb server
    kill $(pgrep rocksdb_server)
    
    sleep 3
  elif [ $db == redis ]; then
    echo "Stopping redis server"
    # Stop redis server
    kill $(pgrep redis-server)

    sleep 10
  fi

  # Remove the temp file for server output
  rm ${outputs_folder}/${test_name_suffix}_server.txt

  # Calculate the average controller time & system time per client
  avg_controller_time=$(echo "$controller_times / $n_clients" | bc -l | awk '{printf "%.9f", $0}')
  avg_system_time=$(echo "$system_times / $n_clients" | bc -l | awk '{printf "%.9f", $0}')
  if [ $(echo "$avg_controller_time < 0" | bc) -eq 1 ]; then
    avg_controller_time="0$avg_controller_time"
  fi
  if [ $(echo "$avg_system_time < 0" | bc) -eq 1 ]; then
    avg_system_time="0$avg_system_time"
  fi
  # Write the avg controller and system time to the result csv file
  echo -e "$workload,$controller,$db,$n_clients,$avg_controller_time,$avg_system_time" >> ${outputs_folder}/${results_csv_file}

}

# build controller
echo "Building controller"
pushd $project_root/controller
cmake -S . -B build -D CMAKE_BUILD_TYPE=Release -D ENCRYPTION_ENABLED=ON; cmake --build build -j$(nproc)
popd

# create test_outputs folder to store the output txt files
mkdir -p ${outputs_folder}

# create test outputs csv file
if [ ! -f "${outputs_folder}/${results_csv_file}" ]; then
  touch ${outputs_folder}/${results_csv_file}
  echo -e "workload,controller,db,n_clients,controller_time,system_time" >> ${outputs_folder}/${results_csv_file}
fi

# TESTS with combibations of
#   {1,2,4,8,16,32} clients,
#   {redis, rocksdb} dbs,
#   {workloada_test, workloadb_test, workloadc_test, workloadd_test, workloadf_test} workloads
# clients="1 2 4 8 16 32"
clients="1"
dbs="redis rocksdb"
#workloads="workload_monitor_vanilla workload_monitor_0 workload_monitor_10 workload_monitor_20 workload_monitor_50 workload_monitor_100"
# workloads="workloada_test workloadb_test workloadc_test workloadd_test workloadf_test"
# workloads="workloada_test workloadb_test"
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
