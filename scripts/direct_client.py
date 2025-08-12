import subprocess
import socket
import argparse
import multiprocessing
import time
import os
import sys
import glob
from contextlib import contextmanager

curr_dir = os.path.dirname(os.path.abspath(__file__))
parent_dir = os.path.dirname(curr_dir)
sys.path.insert(0, parent_dir) 
from policy_compiler.helper import safe_open

workload_trace_dir = os.path.join(curr_dir, '..', 'workload_traces')
exit_query = "query(exit)"
msg_header_size = 4

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

def safe_receive(sock, size):
  """Safely receives data from a socket, ensuring that the desired number of bytes are received."""
  data = b""
  total_bytes_received = 0
  while total_bytes_received < size:
    chunk = sock.recv(size - total_bytes_received)
    if not chunk:
      return b""
    data += chunk
    total_bytes_received += len(chunk)
  return data

def load_workload(db_type, db_address, socket_path, workload_name, value_size):
  """Load workload phase - start server and run load queries"""
  
  load_file = os.path.join(workload_trace_dir, f"{workload_name}_load")
  
  # Connect to Unix socket
  client_socket = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
  client_socket.connect(socket_path)
  
  value = generate_value(value_size)
  
  with safe_open(load_file, 'r') as file:
    for line in file:
      if not line.startswith("#"):
        preprocessed_query = process_query(line, value)
        query_encoded = preprocessed_query.encode()
        msg_size = len(query_encoded).to_bytes(msg_header_size, 'big')
        client_socket.sendall(msg_size + query_encoded)
        
        # Read response
        response_size_data = safe_receive(client_socket, msg_header_size)
        response_size = int.from_bytes(response_size_data, 'big')
        response = safe_receive(client_socket, response_size)
  
  # Send exit
  exit_encoded = exit_query.encode()
  exit_size = len(exit_encoded).to_bytes(msg_header_size, 'big')
  client_socket.sendall(exit_size + exit_encoded)
  client_socket.close()

@contextmanager
def timer(time_dict, stage, breakdown):
  """Context manager to time a section of code, conditional on 'breakdown'."""
  if breakdown:
    start_time = time.perf_counter()
    yield
    end_time = time.perf_counter()
    time_dict[stage] += end_time - start_time
  else:
    yield

def send_queries(socket_path, queries, latency_results, time_breakdowns, breakdown):
  # Connect to Unix socket
  client_socket = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
  client_socket.connect(socket_path)
  
  breakdown_dict = {'prep': 0, 'send': 0, 'wait': 0}
  total_latency = 0
  request_count = 0
  
  for query in queries:
    start_time = time.perf_counter()
    
    # Time query preparation
    with timer(breakdown_dict, 'prep', breakdown):
      query_encoded = query.encode()
      msg_size = len(query_encoded).to_bytes(msg_header_size, 'big')
    
    # Time sending
    with timer(breakdown_dict, 'send', breakdown):
      client_socket.sendall(msg_size + query_encoded)
    
    # Time waiting and receiving
    with timer(breakdown_dict, 'wait', breakdown):
      response_size_data = safe_receive(client_socket, msg_header_size)
      response_size = int.from_bytes(response_size_data, 'big')
      response = safe_receive(client_socket, response_size)
    
    end_time = time.perf_counter()
    
    latency = end_time - start_time
    total_latency += latency
    request_count += 1
  
  # Send exit
  exit_encoded = exit_query.encode()
  exit_size = len(exit_encoded).to_bytes(msg_header_size, 'big')
  client_socket.sendall(exit_size + exit_encoded)
  client_socket.close()
  
  if request_count > 0:
    average_latency = total_latency / request_count
    latency_results.append(average_latency)
  
  if breakdown:
    time_breakdowns.append((breakdown_dict['prep'], breakdown_dict['send'], breakdown_dict['wait']))

def create_client_process(socket_path, queries, latency_results, time_breakdowns, breakdown):
  process = multiprocessing.Process(target=send_queries, args=(socket_path, queries, latency_results, time_breakdowns, breakdown))
  process.start()
  return process

def main():
  parser = argparse.ArgumentParser(description='Start a Unix socket client.')
  parser.add_argument('--workload', help='Name of the workload trace', required=True, type=str, choices=get_workload_options())
  parser.add_argument('--db', help='Database type (redis or rocksdb)', required=True, type=str, choices=["redis", "rocksdb"])
  parser.add_argument('--db_address', help='Address of the database server', required=True, type=str)
  parser.add_argument('--socket_path', help='Path to Unix socket', default='/tmp/direct_kv_client.sock', type=str)
  parser.add_argument('--clients', help='Number of clients to spawn', default=1, type=int)
  parser.add_argument('--value_size', help='Size of the value in bytes for PUT queries', default=1024, type=int)
  parser.add_argument('--breakdown', help='Enable breakdown measurements', action='store_true')
  args = parser.parse_args()

  # Start the server for the run phase
  server_process = subprocess.Popen([
      os.path.join(os.path.dirname(os.path.abspath(__file__)), '../controller/build/direct_kv_client'),
      '--db', args.db,
      '--db_address', args.db_address,
      '--socket_path', args.socket_path
  ])

  # Give server time to start
  time.sleep(1)
  
  # Perform the load phase
  load_workload(args.db, args.db_address, args.socket_path, args.workload, args.value_size)

  # Read the run phase
  run_file = os.path.join(workload_trace_dir, f"{args.workload}_run")
  with safe_open(run_file, 'r') as file:
    queries = [line for line in file if not line.startswith("#")]

  preprocessed_queries = preprocess_queries(queries, args.value_size)
  queries_per_client = [preprocessed_queries[i::args.clients] for i in range(args.clients)]
  
  manager = multiprocessing.Manager()
  latency_results = manager.list()
  time_breakdowns = manager.list()
  processes = []
  
  # Start the time measurement before sending the workload
  start_time = time.perf_counter()
  
  for client_queries in queries_per_client:
    process = create_client_process(args.socket_path, client_queries, latency_results, time_breakdowns, args.breakdown)
    processes.append(process)

  # Wait for all client processes to finish
  for process in processes:
    process.join()

  # End the timer after the controller has returned
  end_time = time.perf_counter()
  elapsed_time = end_time - start_time

  # Terminate server
  server_process.terminate()
  server_process.wait()

  if len(latency_results) > 0:
    average_latency = sum(latency_results) / len(latency_results)
    print(f"Average Latency: {average_latency:.6f} seconds")
  else:
    print("Did not gather latency statistics --- experiment failed.")

  if args.breakdown and len(time_breakdowns) > 0:
    total_prep_time = sum(breakdown[0] for breakdown in time_breakdowns)
    total_send_time = sum(breakdown[1] for breakdown in time_breakdowns)
    total_wait_time = sum(breakdown[2] for breakdown in time_breakdowns)
    print(f"Query preparation time: {total_prep_time:.3f} seconds ({total_prep_time/elapsed_time*100:.2f}%)")
    print(f"Unix socket send time: {total_send_time:.3f} seconds ({total_send_time/elapsed_time*100:.2f}%)")
    print(f"Unix socket wait time: {total_wait_time:.3f} seconds ({total_wait_time/elapsed_time*100:.2f}%)")

  print(f"Elapsed time: {elapsed_time:.3f} seconds")

if __name__ == "__main__":
    main()
