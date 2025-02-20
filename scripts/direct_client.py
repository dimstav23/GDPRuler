import subprocess
import argparse
import multiprocessing
import time
import os
import sys
import glob

curr_dir = os.path.dirname(os.path.abspath(__file__))
parent_dir = os.path.dirname(curr_dir)
sys.path.insert(0, parent_dir) 
from policy_compiler.helper import safe_open

workload_trace_dir = os.path.join(curr_dir, '..', 'workload_traces')

def generate_value(size):
    """Generate a string of the specified size in bytes."""
    return 'x' * size

def process_query(query, value):
  """Replace 'VAL' with the dummy value in the query."""
  return query.replace('VAL', value)

def preprocess_queries(queries, value_size):
  """Preprocess all queries, replacing 'VAL' with a dummy value."""
  value = generate_value(value_size)
  return [process_query(query, value) for query in queries]

def get_workload_options():
  workload_files = glob.glob(os.path.join(workload_trace_dir, '*_run'))
  return [os.path.basename(f).replace('_run', '') for f in workload_files]

def load_workload(db_type, db_address, workload_name, value_size):
  load_file = os.path.join(workload_trace_dir, f"{workload_name}_load")
  process_args = [os.path.join(os.path.dirname(os.path.abspath(__file__)), '../controller/build/direct_kv_client')]
  process_args += ['--db', db_type]
  process_args += ['--db_address', db_address]

  client_process = subprocess.Popen(process_args, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

  value = generate_value(value_size)

  with safe_open(load_file, 'r') as file:
    for line in file:
      if not line.startswith("#"):
        preprocessed_query = process_query(line, value)
        client_process.stdin.write(preprocessed_query.encode())
        client_process.stdin.flush()
        client_process.stdout.readline()  # Read the response

  client_process.terminate()
  client_process.wait()

def send_queries(db_type, db_address, queries, latency_results):
  # Open the client process
  process_args = [os.path.join(os.path.dirname(os.path.abspath(__file__)), '../controller/build/direct_kv_client')]
  process_args += ['--db', db_type]
  process_args += ['--db_address', db_address]

  client_process = subprocess.Popen(process_args, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

  total_latency = 0
  request_count = 0

  for query in queries:
    start_time = time.perf_counter()  # Start the timer

    # Send the query to the client process
    client_process.stdin.write(query.encode())
    client_process.stdin.flush()

    # Read the response from the client process
    response = client_process.stdout.readline().strip()

    end_time = time.perf_counter()  # End the timer

    # Calculate and accumulate the latency
    latency = end_time - start_time
    total_latency += latency
    request_count += 1

  # Close the client process
  client_process.terminate()
  client_process.wait()

  # Save the average latency
  if request_count > 0:
    average_latency = total_latency / request_count
    latency_results.append(average_latency)

def create_client_process(db_type, db_address, queries, latency_results):
  process = multiprocessing.Process(target=send_queries, args=(db_type, db_address, queries, latency_results))
  process.start()
  return process

def main():
  get_workload_options()
  parser = argparse.ArgumentParser(description='Start a client.')
  parser.add_argument('--workload', help='Name of the workload trace', required=True, type=str, choices=get_workload_options())
  parser.add_argument('--db', help='Database type (redis or rocksdb)', required=True, type=str, choices=["redis", "rocksdb"])
  parser.add_argument('--db_address', help='Address of the database server', required=True, type=str)
  parser.add_argument('--clients', help='Number of clients to spawn', default=1, type=int)
  parser.add_argument('--value_size', help='Size of the value in bytes for PUT queries', default=64, type=int)
  args = parser.parse_args()

  # Perform the load phase of the workload
  load_workload(args.db, args.db_address, args.workload, args.value_size)

  # Read the run phase of the workload
  run_file = os.path.join(workload_trace_dir, f"{args.workload}_run")
  with safe_open(run_file, 'r') as file:
    queries = [line for line in file if not line.startswith("#")]

  # Preprocess all queries to expand the dummy value field
  preprocessed_queries = preprocess_queries(queries, args.value_size)

  # Split queries among clients
  queries_per_client = [preprocessed_queries[i::args.clients] for i in range(args.clients)]

  # Start the time measurement before sending the workload
  start_time = time.perf_counter()

  manager = multiprocessing.Manager()
  latency_results = manager.list()
  processes = []
  for client_queries in queries_per_client:
    process = create_client_process(args.db, args.db_address, client_queries, latency_results)
    processes.append(process)

  # Wait for all client processes to finish
  for process in processes:
    process.join()

  # End the timer after all clients have finished
  end_time = time.perf_counter()
  elapsed_time = end_time - start_time

  # Calculate and print the average latency
  if len(latency_results) > 0:
    average_latency = sum(latency_results) / len(latency_results)
    print(f"Average Latency: {average_latency:.6f} seconds")
  else:
    print("Did not gather latency statistics --- experiment failed.")

  # Print the overall elapsed time
  print(f"Elapsed time: {elapsed_time:.3f} seconds")

if __name__ == "__main__":
  main()
