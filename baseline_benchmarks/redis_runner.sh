#!/bin/bash

default_policy_cfg=../configs/owner_policy.json
workload_folder=../workload_traces
repeats=3
workloads="workloada workloadb workloadc workloadd workloadf"
redis_server_path=../KVs/redis/src

# server_address="tcp://192.168.122.48:6379"
# server_address="tcp://192.168.122.48:6379"

mkdir -p results
rm -rf ./dump.rdb
rm -rf ${redis_server_path}/dump.rdb

# Vanilla native version
for workload in $workloads; do
  controller_times=0
  system_times=0
  for rep in $(seq 1 $repeats); do
    echo "Running Vanilla version with $workload... (repeat: $rep)"
    ${redis_server_path}/redis-server &> /dev/null &
    redis_pid=$!
    sleep 3
    python3 ../native_ctl.py --workload ${workload_folder}/${workload} --db redis > ./results/native_${workload}_${rep}
    controller_time=$(grep "Controller time:" ./results/native_${workload}_${rep} | awk '{print $3}')
    system_time=$(grep "System time:" ./results/native_${workload}_${rep} | awk '{print $3}')
    controller_times=$(echo "$controller_times + $controller_time" | bc -l)
    system_times=$(echo "$system_times + $system_time" | bc -l)
    kill $redis_pid
    sleep 3
    rm -rf ./dump.rdb
    rm -rf ${redis_server_path}/dump.rdb
  done
  avg_controller_time=$(echo "$controller_times / $repeats" | bc -l | awk '{printf "%.9f", $0}')
  avg_system_time=$(echo "$system_times / $repeats" | bc -l | awk '{printf "%.9f", $0}')
  if [ $(echo "$avg_controller_time < 0" | bc) -eq 1 ]; then
    avg_controller_time="0$avg_controller_time"
  fi
  if [ $(echo "$avg_system_time < 0" | bc) -eq 1 ]; then
    avg_system_time="0$avg_system_time"
  fi
  echo "$workload,controller_time,$avg_controller_time" >> ./results/native_average.csv
  echo "$workload,system_time,$avg_system_time" >> ./results/native_average.csv
done

# GDPRuler native version
for workload in $workloads; do
  controller_times=0
  system_times=0
  for rep in $(seq 1 $repeats); do
    echo "Running GDPR version with $workload... (repeat: $rep)"
    ${redis_server_path}/redis-server &> /dev/null &
    redis_pid=$!
    sleep 3
    python3 ../GDPRuler.py -c ${default_policy_cfg} --workload ${workload_folder}/${workload} --db redis > ./results/gdpr_${workload}_${rep}
    controller_time=$(grep "Controller time:" ./results/gdpr_${workload}_${rep} | awk '{print $3}')
    system_time=$(grep "System time:" ./results/gdpr_${workload}_${rep} | awk '{print $3}')
    controller_times=$(echo "$controller_times + $controller_time" | bc -l)
    system_times=$(echo "$system_times + $system_time" | bc -l)
    kill $redis_pid
    sleep 3
    rm -rf ./dump.rdb
    rm -rf ${redis_server_path}/dump.rdb
  done 
  avg_controller_time=$(echo "$controller_times / $repeats" | bc -l | awk '{printf "%.9f", $0}')
  avg_system_time=$(echo "$system_times / $repeats" | bc -l | awk '{printf "%.9f", $0}')
  if [ $(echo "$avg_controller_time < 0" | bc) -eq 1 ]; then
    avg_controller_time="0$avg_controller_time"
  fi
  if [ $(echo "$avg_system_time < 0" | bc) -eq 1 ]; then
    avg_system_time="0$avg_system_time"
  fi
  echo "$workload,controller_time,$avg_controller_time" >> ./results/gdpr_average.csv
  echo "$workload,system_time,$avg_system_time" >> ./results/gdpr_average.csv
done

# # SEV GDPRuler version
# for workload in $workloads; do
#   controller_times=0
#   system_times=0
#   for rep in $(seq 1 $repeats); do
#     ${redis_server_path}/redis_server &
#     redis_pid=$!
#     python3 ../GDPRuler.py -c ${default_policy_cfg} -w ${workload_folder}/${workload} > ./results/sec_gdpr_${workload}_${rep}
#     controller_time=$(grep "Controller time:" ./results/sec_gdpr_${workload}_${rep} | awk '{print $3}')
#     system_time=$(grep "System time:" ./results/sec_gdpr_${workload}_${rep} | awk '{print $3}')
#     controller_times=$(echo "$controller_times + $controller_time" | bc)
#     system_times=$(echo "$system_times + $system_time" | bc)
#     kill $redis_pid
#     rm -rf ./dump.rdb
#     rm -rf ${redis_server_path}/dump.rdb
#   done
#   avg_controller_time=$(echo "$controller_times / $repeats" | bc)
#   avg_system_time=$(echo "$system_times / $repeats" | bc)
#   echo "$workload,controller_time,$avg_controller_time" >> ./results/sec_gdpr_average.csv
#   echo "$workload,system_time,$avg_system_time" >> ./results/sec_gdpr_average.csv
# done