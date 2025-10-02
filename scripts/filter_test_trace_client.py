import socket
import argparse
import multiprocessing
import time
import os
import sys
import glob
import json
from contextlib import contextmanager


curr_dir = os.path.dirname(os.path.abspath(__file__))
parent_dir = os.path.dirname(curr_dir)
sys.path.insert(0, parent_dir) 
from policy_compiler.helper import safe_open
from policy_compiler.policy_config import parse_user_policy


workload_trace_dir = os.path.join(curr_dir, '..', 'workload_traces')
exit_query="query(exit)\n"
msg_header_size=4


def get_expected_outputs():
  """Return dictionary mapping query patterns to expected outputs for unit tests"""
  expected = {
    # Loading Phase
    'query(PUT("key0","VAL"))': '1',
    'query(PUT("gdpr1","VAL"))&sessionKey("user1")&objOrig("src1")&monitor("false")&objObjections("purpose3")&objPur("purpose0,purpose1,purpose2")&objShare("user0")&objExp("0")': '1',
    'query(PUT("gdpr2","VAL"))&sessionKey("user1")&objOrig("src1")&monitor("false")&objObjections("purpose3")&objPur("purpose0,purpose1,purpose2")&objShare("user2")&objExp("0")': '1',
    'query(PUT("gdpr3","VAL"))&sessionKey("user1")&objOrig("src1")&monitor("false")&objObjections("purpose3")&objPur("purpose0,purpose1,purpose2,purpose3")&objShare("user0,user2")&objExp("0")': '1',
    'query(PUT("gdpr4","VAL"))&sessionKey("user1")&objOrig("src1")&monitor("true")&objObjections("purpose4")&objPur("purpose0,purpose1,purpose2,purpose3")&objShare("user0,user2")&objExp("0")': '1',
    'query(PUT("key1","VAL"))&sessionKey("user0")&objOrig("src1")&monitor("false")&objObjections("purpose3")&objPur("purpose0,purpose1,purpose2")&objShare("user1")&objExp("0")': '1',
    'query(PUT("key2","VAL"))&sessionKey("user0")&objOrig("src1")&monitor("false")&objObjections("purpose3")&objPur("purpose1,purpose2")&objShare("user2")&objExp("0")': '1',
    'query(PUT("key3","VAL"))&sessionKey("user0")&objOrig("src1")&monitor("false")&objObjections("purpose3")&objPur("purpose2,purpose3")&objShare("user1,user2")&objExp("0")': '1',
    'query(PUT("key4","VAL"))&sessionKey("user0")&objOrig("src1")&monitor("true")&objObjections("purpose4")&objPur("purpose0,purpose1,purpose2,purpose3")&objShare("user1,user2")&objExp("0")': '1',
    'query(PUT("key5","VAL"))&sessionKey("user0")&objOrig("src1")&monitor("true")&objObjections("purpose4")&objPur("purpose0,purpose1,purpose2,purpose3")&objShare("user1,user2")&objExp("0")': '1',
    
    # Execution Phase - GET Tests
    'query(GET("gdpr1"))&sessionKey("user1")&objPurIs("purpose1")': 'VAL',     # Owner, purpose1 allowed
    'query(GET("gdpr1"))&sessionKey("user1")&objPurIs("purpose3")': '0',       # Owner, purpose3 objected  
    'query(GET("gdpr2"))&sessionKey("user2")&objPurIs("purpose1")': 'VAL',     # Shared user, purpose1 allowed
    'query(GET("gdpr1"))&sessionKey("user2")&objPurIs("purpose1")': '0',       # user2 not authorized for gdpr1
    'query(GET("key0"))': 'VAL',                                               # Default session (user0), default policy
    'query(GET("key0"))&objPurIs("purpose1,purpose2")': 'VAL',                # Both allowed in default policy (purpose0-31)
    'query(GET("key0"))&objPurIs("purpose3")': 'VAL',                         # purpose3 allowed in default policy
    'query(GET("key0"))&objPurIs("purpose32")': '0',                          # purpose32 objected in default policy (purpose32-63)
    
    # PUTM Tests
    'query(PUTM("gdpr1"))&sessionKey("user1")&objPurIs("purpose1")&objPur("purpose5,purpose6")': '6',  # Owner can update
    'query(PUTM("gdpr1"))&sessionKey("user2")&objPurIs("purpose1")&objPur("purpose5,purpose6")': '7',  # Non-owner cannot update  
    'query(PUTM("gdpr"))&sessionKey("user1")&objPurIs("purpose1")&objPur("purpose5,purpose6")': '6',   # Updates gdpr1,2,3,4 → all now have purpose5,6
    
    # GETM Tests - AFTER PUTM changed gdpr* purposes to [purpose5,purpose6]
    'query(GETM("gdpr","data"))&sessionKey("user1")&objPurIs("purpose1,purpose2")': '5',             # GETMFAILED - no gdpr* keys have purpose1,2 anymore
    'query(GETM("gdpr","metadata"))&sessionKey("user1")&objPurIs("purpose1,purpose2")': '5',         # GETMFAILED - no gdpr* keys match
    'query(GETM("gdpr","data"))&sessionKey("user3")&objPurIs("purpose1")': '5',                      # GETMFAILED - user3 unauthorized
    'query(GETM("key","data"))&sessionKey("user0")&objPurIs("purpose1,purpose2")': 'VAL|VAL|VAL|VAL|VAL', # key* still have original purposes
    'query(GETM("key","data"))&sessionKey("user0")&objPurIs("purpose5,purpose6")': 'VAL',             # only "key", the rest key* don't have purpose5,6 (PUTM didn't affect key*)
    
    # PUTC Tests
    'query(PUTC("gdpr1","VAL"))&sessionKey("user1")&objOrig("src2")&monitor("false")&objObjections("purpose3")&objPur("purpose0,purpose1,purpose2")&objShare("user0,user2")&objExp("0")': '8',  # Owner can use PUTC
    'query(PUTC("gdpr1","VAL"))&sessionKey("user2")&objOrig("src2")&monitor("false")&objObjections("purpose3")&objPur("purpose0,purpose1,purpose2")&objShare("user0,user2")&objExp("0")': '9',  # Non-owner cannot use PUTC
    'query(PUTC("gdpr5","VAL"))&sessionKey("user1")&objOrig("src1")&monitor("true")&objObjections("purpose3")&objPur("purpose0,purpose1,purpose2,purpose3")&objShare("user0,user2,user4")&objExp("0")': '8',  # Creates new key gdpr5
    'query(PUTC("gdpr5","VAL"))&sessionKey("user1")&objOrig("src2")&monitor("false")&objObjections("purpose3")&objPur("purpose0,purpose1,purpose2")&objShare("user0,user2")&objExp("0")': '8',     # Updates existing gdpr5
    'query(PUTC("gdpr5","VAL"))&sessionKey("user1")&objOrig("src3")&monitor("false")&objObjections("purpose3")&objPur("purpose0,purpose1")&objShare("user0")&objExp("0")': '8',                   # Updates gdpr5 again
    
    # DELETE Tests
    'query(DELETE("gdpr4"))&sessionKey("user1")': '3',  # Owner can delete
    'query(DELETE("gdpr1"))&sessionKey("user2")': '4',  # Non-owner cannot delete
    
    # Complex Tests - Track state changes
    'query(PUT("cascade1","VAL"))&sessionKey("user1")&objPur("purpose1,purpose2")&objShare("user2")&objExp("0")': '1',               # Creates cascade1 with purpose1,2
    'query(PUTM("cascade1"))&sessionKey("user1")&objPur("purpose1,purpose2,purpose3")': '6',                                        # Updates cascade1 to have purpose1,2,3
    'query(GET("cascade1"))&sessionKey("user2")&objPurIs("purpose3")': 'VAL',                                                      # user2 can now access with purpose3
    'query(PUT("objtest1","VAL"))&sessionKey("user1")&objPur("purpose1,purpose2")&objObjections("purpose2")&objExp("0")': '1',    # Creates objtest1, purpose2 objected
    'query(GET("objtest1"))&sessionKey("user1")&objPurIs("purpose2")': '0',                                                       # purpose2 is objected
    'query(PUT("sharetest1","VAL"))&sessionKey("user1")&objPur("purpose1,purpose2")&objShare("user2,user3")&objExp("0")': '1',   # Creates sharetest1, shared with user2,3
    'query(GET("sharetest1"))&sessionKey("user2")&objPurIs("purpose1")': 'VAL',                                                   # user2 is shared
    'query(GET("sharetest1"))&sessionKey("user4")&objPurIs("purpose1")': '0',                                                     # user4 not shared
    
    # Multi-User Tests
    'query(PUT("multitest1","VAL"))&sessionKey("user1")&objPur("purpose1,purpose2,purpose3")&objShare("user2")&objExp("0")': '1', # Creates multitest1
    'query(GET("multitest1"))&sessionKey("user1")&objPurIs("purpose1,purpose2")': 'VAL',                                         # Owner access
    'query(GET("multitest1"))&sessionKey("user2")&objPurIs("purpose1,purpose2")': 'VAL',                                         # Shared access
    'query(GET("multitest1"))&sessionKey("user3")&objPurIs("purpose1")': '0',                                                    # user3 not shared
    'query(PUTM("multitest1"))&sessionKey("user1")&objPur("purpose1,purpose2,purpose4")': '6',                                  # Updates purposes to 1,2,4
    'query(GET("multitest1"))&sessionKey("user2")&objPurIs("purpose4")': 'VAL',                                                  # user2 can access with new purpose4
    
    # Purpose Limitation Tests
    'query(PUT("purtest1","VAL"))&sessionKey("user1")&objPur("purpose1")&objObjections("purpose2,purpose3")&objExp("0")': '1',  # Only purpose1 allowed, 2,3 objected
    'query(GET("purtest1"))&sessionKey("user1")&objPurIs("purpose1")': 'VAL',                                                   # purpose1 allowed
    'query(GET("purtest1"))&sessionKey("user1")&objPurIs("purpose2")': '0',                                                     # purpose2 objected
    'query(GET("purtest1"))&sessionKey("user1")&objPurIs("purpose1,purpose2")': '0',                                            # Mixed allowed/objected = fail
    'query(GET("purtest1"))&sessionKey("user1")&objPurIs("purpose4")': '0',                                                     # purpose4 not in allowed list
    
    # Bulk Operations
    'query(PUT("bulk1","VAL"))&sessionKey("user1")&objPur("purpose1,purpose2")&objShare("user2")&objExp("0")': '1',             # Creates bulk1
    'query(PUT("bulk2","VAL"))&sessionKey("user1")&objPur("purpose2,purpose3")&objShare("user3")&objExp("0")': '1',             # Creates bulk2  
    'query(PUT("bulk3","VAL"))&sessionKey("user2")&objPur("purpose1,purpose3")&objShare("user1")&objExp("0")': '1',             # Creates bulk3 (user2 owns)
    'query(GETM("bulk","data"))&sessionKey("user1")&objPurIs("purpose1,purpose2")': 'VAL',                                      # Gets bulk1 (user1 owns) + bulk3 (shared)
    'query(GETM("bulk","data"))&sessionKey("user2")&objPurIs("purpose1")': 'VAL|VAL',                                               # Gets bulk3 (user2 owns) + bulk1 (shared)
    'query(PUTM("bulk"))&sessionKey("user1")&objPur("purpose1,purpose2,purpose4")': '6',                                       # Updates bulk1,bulk2 only (user1 owns)
    
    # Ownership Transfer (PUTC cannot transfer ownership)
    'query(PUT("transfer1","VAL"))&sessionKey("user1")&objPur("purpose1,purpose2")&objShare("user2")&objExp("0")': '1',         # Creates transfer1
    'query(PUTC("transfer1","VAL"))&sessionKey("user2")&objOrig("src2")&objPur("purpose2,purpose3")&objShare("user1")&objExp("0")': '9',  # user2 cannot use PUTC (not owner)
    'query(GET("transfer1"))&sessionKey("user2")&objPurIs("purpose2")': 'VAL',                                                  # user2 still shared, can access original
    
    # Error Conditions
    'query(GET("nonexistent"))&sessionKey("user1")&objPurIs("purpose1")': '0',          # Key doesn't exist
    'query(PUT("errortest1","VAL"))&sessionKey("user1")&objPur("purpose1")&objShare("user2")&objExp("0")': '1',  # Creates errortest1
    'query(GET("errortest1"))': '0',                                                  # Default session (user0), uses default policy but purposes are not all included
    'query(GET("errortest1"))&sessionKey("user1")': '0',                             # Owner access but not with proper allowed purposes
    'query(DELETE("errortest1"))&sessionKey("user2")': '4',                            # user2 cannot delete (not owner)
    'query(DELETE("errortest1"))&sessionKey("user1")': '3',                            # Owner can delete
    'query(GET("errortest1"))&sessionKey("user1")&objPurIs("purpose1")': '0',          # Key deleted, no longer exists
    
    # Final Validation
    'query(PUT("final1","VAL"))&sessionKey("user1")&objOrig("src1")&monitor("true")&objObjections("purpose4,purpose5")&objPur("purpose0,purpose1,purpose2,purpose3")&objShare("user0,user2,user3")&objExp("0")': '1',  # Creates final1
    'query(GET("final1"))&sessionKey("user1")&objPurIs("purpose1,purpose2,purpose3")': 'VAL',  # Owner, all purposes allowed
    'query(GET("final1"))&sessionKey("user2")&objPurIs("purpose1,purpose2")': 'VAL',           # Shared, purposes allowed
    'query(GET("final1"))&sessionKey("user3")&objPurIs("purpose3")': 'VAL',                    # Shared, purpose allowed
    'query(GET("final1"))&sessionKey("user1")&objPurIs("purpose4")': '0',                      # purpose4 objected
    'query(GET("final1"))&sessionKey("user1")&objPurIs("purpose5")': '0',                      # purpose5 objected
    'query(GETM("final","data"))&sessionKey("user1")&objPurIs("purpose1,purpose2,purpose3")': 'VAL',  # Gets final1 data
  }
  return expected



def normalize_query(query):
  """Normalize query by removing whitespace and newlines for matching"""
  return query.strip().replace('\n', '')


def get_expected_output(query, expected_dict, value_placeholder):
  """Get expected output for a query, replacing VAL placeholder if needed"""
  normalized_query = normalize_query(query)
  # Replace the actual value back with VAL for lookup
  if value_placeholder != 'VAL':
    normalized_query = normalized_query.replace(f'"{value_placeholder}"', '"VAL"')
  
  expected = expected_dict.get(normalized_query, 'UNKNOWN')
  
  # Replace VAL in expected output with actual value if needed
  if expected == 'VAL' and value_placeholder != 'VAL':
    return value_placeholder
  elif 'VAL' in expected and value_placeholder != 'VAL':
    return expected.replace('VAL', value_placeholder)
  
  return expected


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
  
  # Read the exit response (but don't print for loading phase)
  try:
    response_size_data = safe_receive(client_socket, msg_header_size)
    if response_size_data:
      response_size = int.from_bytes(response_size_data, 'big')
      response = safe_receive(client_socket, response_size)
      # Don't print loading phase timing
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


def send_queries(server_address, server_port, queries, latency_results, time_breakdowns, config_path, client_num, breakdown, original_queries, value_size):
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
  
  # Get expected outputs dictionary
  expected_dict = get_expected_outputs()
  actual_value = generate_value(value_size)


  # Read the contents of the workload file line by line
  for i, query in enumerate(queries):
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
      
      # Get expected output for comparison  
      original_query = original_queries[i] if i < len(original_queries) else query
      expected = get_expected_output(original_query, expected_dict, actual_value)
      actual = response.decode()
      
      # Print both actual and expected
      status = "✓" if actual.strip() == expected.strip() else "✗"
      print(f"[{status}] Actual: {actual.strip()}, Expected: {expected}")
      if actual.strip() != expected.strip():
        print(f"[Failed {i}th query : {query[:-1]}]")
      
    end_time = time.perf_counter() # End the timer
    # Calculate and accumulate the latency
    latency = end_time - start_time
    total_latency += latency
    request_count += 1


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
  
  # Save the average latency
  if request_count > 0:
    average_latency = total_latency / request_count
    latency_results.append(average_latency)


  if breakdown:
    time_breakdowns.append((breakdown_dict['prep'], breakdown_dict['send'], breakdown_dict['wait']))


def create_client_process(server_address, server_port, queries, latency_results, time_breakdowns, config_path, client_num, breakdown, original_queries, value_size):
  process = multiprocessing.Process(target=send_queries, args=(server_address, server_port, queries, latency_results, time_breakdowns, config_path, client_num, breakdown, original_queries, value_size))
  process.start()
  return process


def main():
  address = "127.0.0.1"
  port = 1312
  workload = "filter_test_trace"
  value_size = 64
  config = "./evaluation/configs"
  clients = 1
  breakdown = False
  load_workload(address, port, workload, value_size, config)


  # Read the run phase of the workload
  run_file = os.path.join(workload_trace_dir, f"{workload}_run")
  with safe_open(run_file, 'r') as file:
    original_queries = [line for line in file if not line.startswith("#")]


  # Preprocess all queries to expand the dummy value field
  preprocessed_queries = preprocess_queries(original_queries, value_size)


  # Split queries among clients
  queries_per_client = [preprocessed_queries[i::clients] for i in range(clients)]
  original_per_client = [original_queries[i::clients] for i in range(clients)]


  manager = multiprocessing.Manager()
  latency_results = manager.list()
  time_breakdowns = manager.list()
  processes = []
  
  # Start the time measurement before sending the workload
  start_time = time.perf_counter()
  
  for i, (client_queries, client_originals) in enumerate(zip(queries_per_client, original_per_client)):
    process = create_client_process(address, port, client_queries, latency_results, time_breakdowns, config, i, breakdown, client_originals, value_size)
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
  if breakdown and len(time_breakdowns) > 0:
    total_prep_time = sum(breakdown[0] for breakdown in time_breakdowns)
    total_send_time = sum(breakdown[1] for breakdown in time_breakdowns)
    total_wait_time = sum(breakdown[2] for breakdown in time_breakdowns)
    print(f"Query preparation time: {total_prep_time:.3f} seconds ({total_prep_time/elapsed_time*100:.2f}%)")
    print(f"Network send time: {total_send_time:.3f} seconds ({total_send_time/elapsed_time*100:.2f}%)")
    print(f"Wait and receive time: {total_wait_time:.3f} seconds ({total_wait_time/elapsed_time*100:.2f}%)")


  print(f"Elapsed time: {elapsed_time:.3f} seconds (100%)")  
  
if __name__ == "__main__":
  main()
