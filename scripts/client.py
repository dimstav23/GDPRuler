import socket
import argparse
import multiprocessing
import time
import os
import sys
import glob
import json
from contextlib import contextmanager

# Add imports for statistics
import statistics

curr_dir = os.path.dirname(os.path.abspath(__file__))
parent_dir = os.path.dirname(curr_dir)
sys.path.insert(0, parent_dir) 
from policy_compiler.helper import safe_open
from policy_compiler.policy_config import parse_user_policy

workload_trace_dir = os.path.join(curr_dir, '..', 'workload_traces')
exit_query="query(exit)\n"
msg_header_size=4

GET_FAILED       = "0"
GET_META_FAILED  = "1"
PUT_SUCCESS      = "2"
PUT_FAILED       = "3"
PUT_META_SUCCESS = "4"
PUT_META_FAILED  = "5"
DELETE_SUCCESS   = "6"
DELETE_FAILED    = "7"
GETM_EMPTY       = "8"
PUTM_SUCCESS     = "9"
PUTM_FAILED      = "10"
PUTM_EMPTY       = "11"
DELETEM_SUCCESS  = "12"
DELETEM_FAILED   = "13"
DELETEM_EMPTY    = "14"
PUTC_SUCCESS     = "15"
PUTC_FAILED      = "16"
GET_LOGS_FAILED  = "17"
INVALID_COMMAND  = "18"
UNKNOWN_ERROR    = "19"

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
  """Load workload phase - start server and run load queries"""
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
  
  try:
    response_size_data = safe_receive(client_socket, msg_header_size)
    if response_size_data:
      response_size = int.from_bytes(response_size_data, 'big')
      response = safe_receive(client_socket, response_size)
  except:
    pass
    
  client_socket.close()
  print(f"Workload {workload_name} loaded successfully.")

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

@contextmanager
def timer(time_dict, stage, breakdown):
  """Context manager to time a section of code, conditional on 'breakdown'."""
  if breakdown:
    start_time = time.perf_counter()
    yield
    end_time = time.perf_counter()
    time_dict[stage] += end_time - start_time
  else:
    yield  # If not breakdown, execute the code but don't measure time

def send_drain_request(server_address, server_port, config_path):
  """Send drain request to controller and measure the time"""
  try:
    client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    client_socket.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    client_socket.connect((server_address, server_port))
    
    # Load and send default policy
    if config_path != "no_cfg":
      default_policy = load_config(config_path, 0)
      if not send_default_policy(client_socket, default_policy):
        print(f"On Drain: Failed to set default policy for client {0}")
        client_socket.close()
        return
    
    # Send drain query
    drain_query = "query(drain)\n"
    query_encoded = drain_query.encode()
    msg_size = len(query_encoded).to_bytes(msg_header_size, 'big')
    
    start_time = time.perf_counter()
    client_socket.sendall(msg_size + query_encoded)
    
    # Receive response
    response_size_data = safe_receive(client_socket, msg_header_size)
    if response_size_data:
      response_size = int.from_bytes(response_size_data, 'big')
      response = safe_receive(client_socket, response_size)
        
    end_time = time.perf_counter()
    drain_time = end_time - start_time
    
    client_socket.close()
    return drain_time, response.decode() if response else "No response"
      
  except Exception as e:
      return None, f"Error during drain: {str(e)}"

def send_queries(server_address, server_port, queries, latency_results, time_breakdowns, config_path, client_num, breakdown, workload_name, op_results):
  # Enable per-operation latency tracking only for GDPR workloads
  track_ops = workload_name.startswith('gdpr_')
  op_latencies = {} if track_ops else None
    
  # Open a connection to the server
  client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
  client_socket.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
  client_socket.connect((server_address, server_port))

  # Load and send default policy
  if config_path != "no_cfg":
    default_policy = load_config(config_path, client_num)
    if not send_default_policy(client_socket, default_policy):
      print(f"Failed to set default policy for client {client_num}")
      client_socket.close()
      return

  total_latency = 0
  request_count = 0
  breakdown_dict = {'prep': 0, 'send': 0, 'wait': 0}

  # Statistics tracking
  stats = {
    'failed': 0,
    'empty': 0, 
    'success': 0,
    'binary': 0,
    'failed_queries': [],
    'empty_queries': []
  }
  # Define error code sets
  FAILED_CODES = {GET_FAILED, GET_META_FAILED, PUT_FAILED, PUT_META_FAILED, 
                  PUTM_FAILED, DELETE_FAILED, DELETEM_FAILED, PUTC_FAILED, 
                  GET_LOGS_FAILED, INVALID_COMMAND, UNKNOWN_ERROR}
  EMPTY_CODES = {GETM_EMPTY, PUTM_EMPTY, DELETEM_EMPTY}
  SUCCESS_CODES = {PUT_SUCCESS, PUT_META_SUCCESS, DELETE_SUCCESS, PUTM_SUCCESS, 
                   DELETEM_SUCCESS, PUTC_SUCCESS}
  
  # Read the contents of the workload file line by line
  for i, query in enumerate(queries):
    # For GDPR: extract operation name for latency grouping
    if track_ops and 'query(' in query:
      op_name = query.split('query(')[1].split('(')[0]

    start_time = time.perf_counter() # Start the timer
    # Send each line to the server with message size header
    with timer(breakdown_dict, 'prep', breakdown):
      query_encoded = query.encode()
      msg_size = len(query_encoded).to_bytes(msg_header_size, 'big')
    with timer(breakdown_dict, 'send', breakdown):
      client_socket.sendall(msg_size + query_encoded)
    with timer(breakdown_dict, 'wait', breakdown):
      # Receive the server's response with message size header
      response_size_data = safe_receive(client_socket, msg_header_size)
      response_size = int.from_bytes(response_size_data, 'big')
      response = safe_receive(client_socket, response_size)
      if breakdown:
        try:
          actual = response.decode('utf-8')
          is_binary = False
        except UnicodeDecodeError:
          # Binary metadata response - convert to hex string for display
          actual = response.hex()
          is_binary = True

        # Categorize the response
        if is_binary:
          stats['binary'] += 1
          stats['success'] += 1  # Binary responses are valid metadata
        elif actual.strip() in FAILED_CODES:
          stats['failed'] += 1
          stats['failed_queries'].append((i, query.strip(), actual.strip()))
          print(f"[FAILED] Query {i}: {actual.strip()}")
          print(f"  Query: {query.strip()}")
        elif actual.strip() in EMPTY_CODES:
          stats['empty'] += 1
          stats['empty_queries'].append((i, query.strip(), actual.strip()))
        elif actual.strip() in SUCCESS_CODES or actual.strip() not in FAILED_CODES:
          # Either explicit success code or data response (like "VAL")
          stats['success'] += 1
      
    end_time = time.perf_counter() # End the timer
    # Calculate and accumulate the latency
    latency = end_time - start_time
    total_latency += latency
    request_count += 1
    
    # Only record per-op latency for GDPR workloads
    if track_ops:
      if op_name not in op_latencies:
        op_latencies[op_name] = []
      op_latencies[op_name].append(latency)

  # Send exit query to the server and READ the response
  exit_msg_size = len(exit_query).to_bytes(msg_header_size, 'big')
  client_socket.sendall(exit_msg_size + exit_query.encode())
  
  # Read the exit response from server
  try:
    response_size_data = safe_receive(client_socket, msg_header_size)
    if response_size_data:
      response_size = int.from_bytes(response_size_data, 'big')
      response = safe_receive(client_socket, response_size)
      if response:
        print(f"[Client {client_num}] {response.decode()}")
  except:
    pass  # Connection might be closed
  
  # Close the connection
  client_socket.close()
  
  # Print statistics
  if breakdown:
    print(f"\n{'='*60}")
    print(f"Client {client_num} Statistics:")
    print(f"{'='*60}")
    print(f"Total queries:    {request_count}")
    print(f"Successful:       {stats['success']} ({stats['success']/request_count*100:.1f}%)")
    print(f"  - Binary meta:  {stats['binary']}")
    print(f"  - Text/Data:    {stats['success'] - stats['binary']}")
    print(f"Failed:           {stats['failed']} ({stats['failed']/request_count*100:.1f}%)")
    print(f"Empty results:    {stats['empty']} ({stats['empty']/request_count*100:.1f}%)")

    if stats['failed'] > 0:
      print(f"\nFailed Query Details:")
      for idx, query, code in stats['failed_queries'][:10]:  # Show first 10
        print(f"  [{idx}] Code {code}: {query[:80]}...")
      if len(stats['failed_queries']) > 10:
        print(f"  ... and {len(stats['failed_queries']) - 10} more")

    if stats['empty'] > 0:
      print(f"\nEmpty Result Queries:")
      for idx, query, code in stats['empty_queries'][:10]:  # Show first 10
        print(f"  [{idx}] Code {code}: {query[:80]}...")
      if len(stats['empty_queries']) > 10:
        print(f"  ... and {len(stats['empty_queries']) - 10} more")

    print(f"{'='*60}\n")
  
  # Save the average latency
  if request_count > 0:
    average_latency = total_latency / request_count
    latency_results.append(average_latency)

  if breakdown:
    time_breakdowns.append((breakdown_dict['prep'], breakdown_dict['send'], breakdown_dict['wait']))
  
  # Append per-client op latencies for GDPR workloads
  if track_ops and op_latencies:
    op_results.append({'client': client_num, 'latencies': op_latencies})

def create_client_process(server_address, server_port, queries, latency_results, time_breakdowns, config_path, client_num, breakdown, workload_name, op_results):
  process = multiprocessing.Process(target=send_queries, args=(server_address, server_port, queries, latency_results, time_breakdowns, config_path, client_num, breakdown, workload_name, op_results))
  process.start()
  return process

def main():
  parser = argparse.ArgumentParser(description='Start a client.')
  parser.add_argument('--config', help='Path to config file or directory containing client configs for the GDPR case. Provide "no_cfg" for passthrough case', required=True, type=str)
  parser.add_argument('--workload', help='Name of the workload trace', required=True, type=str, choices=get_workload_options())
  parser.add_argument('--address', help='IP address of the server to connect', default="127.0.0.1", required=False, type=str)
  parser.add_argument('--port', help='Port of the running server to connect', default=1312, required=False, type=int)
  parser.add_argument('--clients', help='Number of clients to spawn', default=1, type=int)
  parser.add_argument('--value_size', help='Size of the value in bytes for PUT queries', default=1024, type=int)
  parser.add_argument('--breakdown', help='Enable breakdown measurements', action='store_true')
  parser.add_argument('--drain', help='Send drain request after workload completion', action='store_true')
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

  manager = multiprocessing.Manager()
  latency_results = manager.list()
  time_breakdowns = manager.list()
  op_results = manager.list()
  processes = []
  # Start the time measurement before sending the workload
  start_time = time.perf_counter()
  
  for i, client_queries in enumerate(queries_per_client):
    process = create_client_process(args.address, args.port, client_queries, latency_results, time_breakdowns, args.config, i, args.breakdown, args.workload, op_results)
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
    print("Did not gather latency statistics --- experiment failed.")

  # Calculate and print the time breakdown
  if args.breakdown and len(time_breakdowns) > 0:
    total_prep_time = sum(breakdown[0] for breakdown in time_breakdowns)
    total_send_time = sum(breakdown[1] for breakdown in time_breakdowns)
    total_wait_time = sum(breakdown[2] for breakdown in time_breakdowns)
    print(f"Query preparation time: {total_prep_time:.3f} seconds ({total_prep_time/elapsed_time*100:.2f}%)")
    print(f"Network send time: {total_send_time:.3f} seconds ({total_send_time/elapsed_time*100:.2f}%)")
    print(f"Wait and receive time: {total_wait_time:.3f} seconds ({total_wait_time/elapsed_time*100:.2f}%)")

  print(f"Elapsed time: {elapsed_time:.3f} seconds (100%)")

  if args.workload.startswith('gdpr_') and len(op_results) > 0:
    # Aggregate op latencies
    agg = {}
    for client_data in op_results:
      for op_name, latencies in client_data['latencies'].items():
        if op_name not in agg:
          agg[op_name] = []
        agg[op_name].extend(latencies)
    print("="*60)
    print("Per-Operation Latency Stats (GDPR Workload):")
    for op_name in sorted(agg.keys()):
      lat_list = agg[op_name]
      mean = statistics.mean(lat_list)
      std = statistics.stdev(lat_list) if len(lat_list) > 1 else 0.0
      print(f"{op_name}:")
      print(f"  Count: {len(lat_list)}")
      print(f"  Avg Latency: {mean:.6f} s")
      print(f"  Std Dev:    {std:.6f} s")
    print("="*60)

  if args.drain:
    print("=" * 50)
    print("DRAIN PHASE: Flushing logging queues...")
    drain_time, drain_response = send_drain_request(args.address, args.port, args.config)
      
    if drain_time is not None:
      print(f"Drain time: {drain_time:.6f} seconds")
      print(f"Drain response: {drain_response}")
      print(f"Total time (workload + drain): {elapsed_time + drain_time:.3f} seconds")
    else:
      print(f"Drain failed: {drain_response}")
  
  
if __name__ == "__main__":
  main()
