import subprocess
import os
import shutil
import time
import sys
import multiprocessing
from aggregate_results import aggregate_csv_files, delete_temp_csv_files

curr_dir = os.path.dirname(os.path.abspath(__file__))
parent_dir = os.path.dirname(curr_dir)
sys.path.insert(0, parent_dir) 
from common import execute_queries

dbs = ["redis", "rocksdb"]
workload_folder = os.path.join(curr_dir, "../workload_traces")
# clients = [1, 2, 4, 8, 16]
clients = [1, 2, 4, 8, 16]
repeats = 3
workloads = ["workloada", "workloadb", "workloadc", "workloadd", "workloadf"]
# workloads = ["workloada_test", "workloadb_test", "workloadc_test", "workloadd_test", "workloadf_test"]
db_files_dir = "/scratch/dimitrios/data"

# Setup of the server executables, their arguments and the kv client driver
redis_server_path = [os.path.join(curr_dir, "../KVs/redis/src/redis-server")]
redis_port = 6379
redis_server_args = ["--port", f"{redis_port}", "--dir", f"{db_files_dir}", "--protected-mode no"]

rocksdb_server_path = [os.path.join(curr_dir, "../controller/build/rocksdb_server")]
rocksdb_port = 15001
rocksdb_server_args = [f"{rocksdb_port}", f"{db_files_dir}"]

kv_client_driver = [os.path.join(curr_dir, "../controller/build/test_kv_client_driver")]

results_dir = os.path.join(curr_dir, "results")

# Create and open the result files
def create_result_files(db, workload, client_num, rep):
  result_files = []
  result_file_names = []
  for i in range(client_num):
    result_file_name = f"{db}-{workload}-{i+1}_out_of_{client_num}-{rep}.csv"
    result_file_names.append(os.path.join(results_dir, result_file_name))
    result_file = open(os.path.join(results_dir, result_file_name), "w")
    result_files.append(result_file)
  return result_files, result_file_names

# Close the result files
def close_result_files(result_files):
  for result_file in result_files:
    result_file.close()

# Start the KV server
def run_server(db):
  if db == "redis":
    server_proc = subprocess.Popen(redis_server_path + redis_server_args, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
  elif db == "rocksdb":
    server_proc = subprocess.Popen(rocksdb_server_path + rocksdb_server_args, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
  else:
    print("Only redis and rocksdb KV stores are currently supported! Exiting...")
    exit(1)
  return server_proc

# Start the KV client driver
def run_client(db, result_file):
  kv_client_proc = subprocess.Popen(kv_client_driver + ["--db", f"{db}"], stdin=subprocess.PIPE, stdout=result_file, stderr=subprocess.PIPE)
  return kv_client_proc

# Parse the workload and pass it to the respective KV client driver via its stdin
def run_workload(kv_client_proc, workload):
  execute_queries(kv_client_proc, os.path.join(workload_folder, workload))

def run_experiment(db, client_num, workload):
  result_file_names = []
  for rep in range(1, repeats + 1):
    print("-------------------------------------------------------")
    result_files, experiment_result_file_names = create_result_files(db, workload, client_num, rep)
    result_file_names.extend(experiment_result_file_names)

    print(f"Running {db} server with {workload}... (repeat: {rep})")
    server_proc = run_server(db)
    time.sleep(1)

    print(f"Starting {client_num} KV client(s) for {db}")
    kv_client_processes = []
    for i in range(client_num):
      kv_client_proc = run_client(db, result_files[i])
      kv_client_processes.append(kv_client_proc)

    print(f"Executing {workload} for every client")
    workload_processes = []
    for kv_client_proc in kv_client_processes:
      workload_proc = multiprocessing.Process(target=run_workload, args=(kv_client_proc, workload))
      workload_proc.start()
      workload_processes.append(workload_proc)
      
    for workload_proc in workload_processes:
      workload_proc.join()

    print(f"Killing {db} server and erasing the stored data of the executed run")
    server_proc.kill()
    time.sleep(1)
    shutil.rmtree(db_files_dir, ignore_errors=True)
    os.makedirs(db_files_dir, exist_ok=True)
    close_result_files(result_files)
    
    print("-------------------------------------------------------")
  
  # Aggregate the results for all the repeats and delete the temporary result files
  print("-------------------------------------------------------")
  print(f"Aggregating results for {db} running {workload} with {client_num} client(s)")
  aggregate_csv_file_name = f"{db}-{workload}-{client_num}.csv"
  aggregate_csv_files(result_file_names, os.path.join(results_dir, aggregate_csv_file_name), client_num)
  delete_temp_csv_files(result_file_names)
  print("-------------------------------------------------------")

def main():
  shutil.rmtree(db_files_dir, ignore_errors=True)
  os.makedirs(db_files_dir, exist_ok=True)
  os.makedirs(results_dir, exist_ok=True)
  for db in dbs:
    for client_num in clients:
      for workload in workloads:
        run_experiment(db, client_num, workload)

if __name__ == "__main__":
  main()
