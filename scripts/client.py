import socket
import argparse
import multiprocessing
import time
import os
import sys
import glob
import json

curr_dir = os.path.dirname(os.path.abspath(__file__))
parent_dir = os.path.dirname(curr_dir)
sys.path.insert(0, parent_dir) 
from policy_compiler.helper import safe_open
from policy_compiler.policy_config import parse_user_policy

workload_trace_dir = os.path.join(curr_dir, '..', 'workload_traces')
exit_query="query(exit)\n"
msg_header_size=4

def generate_value(size):
    """Generate a string of the specified size in bytes."""
    return 'x' * size

def process_query(query, value):
  """Replace 'VAL' with the dummy value in the query."""
  return query.replace('VAL', value)

def load_config(config_path, client_num):
  if os.path.isdir(config_path):
    config_file = os.path.join(config_path, f"client{client_num}_config.json")
  else:
    config_file = config_path

  with safe_open(config_file, 'r') as f:
    return json.load(f)

def send_default_policy(client_socket, default_policy):
  def_policy = parse_user_policy(default_policy)
  def_policy = def_policy.encode()
  msg_size = len(def_policy).to_bytes(msg_header_size, 'big')
  client_socket.sendall(msg_size + def_policy)

  # Receive acknowledgment
  ack_size_data = safe_receive(client_socket, msg_header_size)
  ack_size = int.from_bytes(ack_size_data, 'big')
  ack = safe_receive(client_socket, ack_size)
  return ack.decode() == "ACK"

def preprocess_queries(queries, value_size):
  """Preprocess all queries, replacing 'VAL' with a dummy value."""
  value = generate_value(value_size)
  return [process_query(query, value) for query in queries]

def get_workload_options():
  workload_files = glob.glob(os.path.join(workload_trace_dir, '*_run'))
  return [os.path.basename(f).replace('_run', '') for f in workload_files]

def load_workload(server_address, server_port, workload_name, value_size, config_path):
  load_file = os.path.join(workload_trace_dir, f"{workload_name}_load")
  client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
  client_socket.connect((server_address, server_port))

  value = generate_value(value_size)

  # Load and send default policy - get the client 0 as default policy
  if config_path != "no_cfg":
    default_policy = load_config(config_path, 0)
    if not send_default_policy(client_socket, default_policy):
      print(f"Failed to set default policy for the workload loader")
      client_socket.close()
      return

  with safe_open(load_file, 'r') as file:
    for line in file:
      if not line.startswith("#"):
        processed_query = process_query(line, value)
        query = processed_query.encode()
        msg_size = len(query).to_bytes(msg_header_size, 'big')
        client_socket.sendall(msg_size + query)
        response_size_data = safe_receive(client_socket, msg_header_size)
        response_size = int.from_bytes(response_size_data, 'big')
        response = safe_receive(client_socket, response_size)

  exit_msg_size = len(exit_query).to_bytes(msg_header_size, 'big')
  client_socket.sendall(exit_msg_size + exit_query.encode())
  client_socket.close()

def safe_receive(socket, size):
    """
    Safely receives data from a socket, ensuring that the desired number of bytes are received.
    Args:
      socket (socket.socket): The socket object used for communication.
      size (int): The number of bytes to receive.
    Returns:
      bytes: The received data, or an empty bytes object if receiving fails.
    """
    data = b""
    total_bytes_received = 0
    while total_bytes_received < size:
      chunk = socket.recv(size - total_bytes_received)
      if not chunk:
        # Failed to receive data or connection closed
        return b""
      data += chunk
      total_bytes_received += len(chunk)
    return data

def send_queries(server_address, server_port, queries, latency_results, config_path, client_num):
    # Open a connection to the server
    client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    client_socket.connect((server_address, server_port))

    # Load and send default policy
    if config_path != "no_cfg":
      default_policy = load_config(config_path, client_num)
      if not send_default_policy(client_socket, default_policy):
        print(f"Failed to set default policy for client {client_num}")
        client_socket.close()
        return

    # Read the contents of the workload file line by line
    total_latency = 0
    request_count = 0

    for query in queries:
      start_time = time.perf_counter()  # Start the timer
      # Send each line to the server with message size header
      query_encoded = query.encode()
      msg_size = len(query_encoded).to_bytes(msg_header_size, 'big')
      client_socket.sendall(msg_size + query_encoded)

      # Receive the server's response with message size header
      response_size_data = safe_receive(client_socket, msg_header_size)
      response_size = int.from_bytes(response_size_data, 'big')
      response = safe_receive(client_socket, response_size)

      end_time = time.perf_counter()  # End the timer

      # Calculate and accumulate the latency
      latency = end_time - start_time
      total_latency += latency
      request_count += 1

    # Send exit query to the server
    exit_msg_size = len(exit_query).to_bytes(msg_header_size, 'big')
    client_socket.sendall(exit_msg_size + exit_query.encode())

    # Close the connection
    client_socket.close()

    # Save the average latency
    if request_count > 0:
      average_latency = total_latency / request_count
      latency_results.append(average_latency)

def create_client_process(server_address, server_port, queries, latency_results, config_path, client_num):
  process = multiprocessing.Process(target=send_queries, args=(server_address, server_port, queries, latency_results, config_path, client_num))
  process.start()
  return process

def main():
  parser = argparse.ArgumentParser(description='Start a client.')
  parser.add_argument('--config', help='Path to config file or directory containing client configs for the GDPR case. Leave empty for passthrough case', required=True, type=str)
  parser.add_argument('--workload', help='Name of the workload trace', required=True, type=str, choices=get_workload_options())
  parser.add_argument('--address', help='IP address of the server to connect', default="127.0.0.1", required=False, type=str)
  parser.add_argument('--port', help='Port of the running server to connect', default=1312, required=False, type=int)
  parser.add_argument('--clients', help='Number of clients to spawn', default=1, type=int)
  parser.add_argument('--value_size', help='Size of the value in bytes for PUT queries', default=1024, type=int)
  args = parser.parse_args()

  # Perform the load phase of the workload
  load_workload(args.address, args.port, args.workload, args.value_size, args.config)

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
  for i, client_queries in enumerate(queries_per_client):
    process = create_client_process(args.address, args.port, client_queries, latency_results, args.config, i)
    processes.append(process)

  # Wait for all client processes to finish
  for process in processes:
    process.join()

  # End the timer after the controller has returned
  end_time = time.perf_counter()
  elapsed_time = end_time - start_time

  # Calculate and print the average latency
  if len(latency_results) > 0:
    average_latency = sum(latency_results) / len(latency_results)
    print(f"Average Latency: {average_latency:.6f} seconds")
  else:
    print("Did not gathered latency statistics --- experiment failed.")

  # Print the overall elapsed time
  print(f"Elapsed time: {elapsed_time:.3f} seconds")

if __name__ == "__main__":
  main()
