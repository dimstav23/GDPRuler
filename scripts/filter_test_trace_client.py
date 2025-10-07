import socket
import argparse
import multiprocessing
import time
import os
import sys
import glob
import json
from contextlib import contextmanager

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
    'query(PUT("key0","VAL"))': PUT_SUCCESS,
    'query(PUT("gdpr1","VAL"))&sessionKey("user1")&objOrig("src1")&monitor("false")&objObjections("purpose3")&objPur("purpose0,purpose1,purpose2")&objShare("user0")&objExp("0")': PUT_SUCCESS,
    'query(PUT("gdpr2","VAL"))&sessionKey("user1")&objOrig("src1")&monitor("false")&objObjections("purpose3")&objPur("purpose0,purpose1,purpose2")&objShare("user2")&objExp("0")': PUT_SUCCESS,
    'query(PUT("gdpr3","VAL"))&sessionKey("user1")&objOrig("src1")&monitor("false")&objObjections("purpose3")&objPur("purpose0,purpose1,purpose2,purpose3")&objShare("user0,user2")&objExp("0")': PUT_SUCCESS,
    'query(PUT("gdpr4","VAL"))&sessionKey("user1")&objOrig("src1")&monitor("true")&objObjections("purpose4")&objPur("purpose0,purpose1,purpose2,purpose3")&objShare("user0,user2")&objExp("0")': PUT_SUCCESS,
    'query(PUT("key1","VAL"))&sessionKey("user0")&objOrig("src1")&monitor("false")&objObjections("purpose3")&objPur("purpose0,purpose1,purpose2")&objShare("user1")&objExp("0")': PUT_SUCCESS,
    'query(PUT("key2","VAL"))&sessionKey("user0")&objOrig("src1")&monitor("false")&objObjections("purpose3")&objPur("purpose1,purpose2")&objShare("user2")&objExp("0")': PUT_SUCCESS,
    'query(PUT("key3","VAL"))&sessionKey("user0")&objOrig("src1")&monitor("false")&objObjections("purpose3")&objPur("purpose2,purpose3")&objShare("user1,user2")&objExp("0")': PUT_SUCCESS,
    'query(PUT("key4","VAL"))&sessionKey("user0")&objOrig("src1")&monitor("true")&objObjections("purpose4")&objPur("purpose0,purpose1,purpose2,purpose3")&objShare("user1,user2")&objExp("0")': PUT_SUCCESS,
    'query(PUT("key5","VAL"))&sessionKey("user0")&objOrig("src1")&monitor("true")&objObjections("purpose4")&objPur("purpose0,purpose1,purpose2,purpose3")&objShare("user1,user2")&objExp("0")': PUT_SUCCESS,
    
    # Execution Phase - GET Tests
    'query(GET("gdpr1"))&sessionKey("user1")&objPurIs("purpose1")': 'VAL',     # Owner, purpose1 allowed
    'query(GET("gdpr1"))&sessionKey("user1")&objPurIs("purpose3")': GET_FAILED,       # Owner, purpose3 objected  
    'query(GET("gdpr2"))&sessionKey("user2")&objPurIs("purpose1")': 'VAL',     # Shared user, purpose1 allowed
    'query(GET("gdpr1"))&sessionKey("user2")&objPurIs("purpose1")': GET_FAILED,       # user2 not authorized for gdpr1
    'query(GET("key0"))': 'VAL',                                               # Default session (user0), default policy
    'query(GET("key0"))&objPurIs("purpose1,purpose2")': 'VAL',                # Both allowed in default policy (purpose0-31)
    'query(GET("key0"))&objPurIs("purpose3")': 'VAL',                         # purpose3 allowed in default policy
    'query(GET("key0"))&objPurIs("purpose32")': GET_FAILED,                          # purpose32 objected in default policy (purpose32-63)
    
    # PUTM Tests
    'query(PUTM("gdpr1"))&sessionKey("user1")&objPurIs("purpose1")&objPur("purpose5,purpose6")': PUTM_SUCCESS,  # Owner can update
    'query(PUTM("gdpr1"))&sessionKey("user2")&objPurIs("purpose1")&objPur("purpose5,purpose6")': PUTM_EMPTY,  # Non-owner cannot update  
    'query(PUTM("gdpr"))&sessionKey("user1")&objPurIs("purpose1")&objPur("purpose5,purpose6")': PUTM_SUCCESS,   # Updates gdpr1,2,3,4 → all now have purpose5,6
    
    # GETM Tests - AFTER PUTM changed gdpr* purposes to [purpose5,purpose6]
    'query(GETM("gdpr","data"))&sessionKey("user1")&objPurIs("purpose1,purpose2")': GETM_EMPTY,             # GETMFAILED - no gdpr* keys have purpose1,2 anymore
    'query(GETM("gdpr","metadata"))&sessionKey("user1")&objPurIs("purpose1,purpose2")': GETM_EMPTY,         # GETMFAILED - no gdpr* keys match
    'query(GETM("gdpr","data"))&sessionKey("user3")&objPurIs("purpose1")': GETM_EMPTY,                      # GETMFAILED - user3 unauthorized
    'query(GETM("key","data"))&sessionKey("user0")&objPurIs("purpose1,purpose2")': 'VAL|VAL|VAL|VAL|VAL', # key* still have original purposes
    'query(GETM("key","data"))&sessionKey("user0")&objPurIs("purpose5,purpose6")': 'VAL',             # only "key", the rest key* don't have purpose5,6 (PUTM didn't affect key*)
    
    # PUTC Tests
    'query(PUTC("gdpr1","VAL"))&sessionKey("user1")&objOrig("src2")&monitor("false")&objObjections("purpose3")&objPur("purpose0,purpose1,purpose2")&objShare("user0,user2")&objExp("0")': PUTC_SUCCESS,  # Owner can use PUTC
    'query(PUTC("gdpr1","VAL"))&sessionKey("user2")&objOrig("src2")&monitor("false")&objObjections("purpose3")&objPur("purpose0,purpose1,purpose2")&objShare("user0,user2")&objExp("0")': PUTC_FAILED,  # Non-owner cannot use PUTC
    'query(PUTC("gdpr5","VAL"))&sessionKey("user1")&objOrig("src1")&monitor("true")&objObjections("purpose3")&objPur("purpose0,purpose1,purpose2,purpose3")&objShare("user0,user2,user4")&objExp("0")': PUTC_SUCCESS,  # Creates new key gdpr5
    'query(PUTC("gdpr5","VAL"))&sessionKey("user1")&objOrig("src2")&monitor("false")&objObjections("purpose3")&objPur("purpose0,purpose1,purpose2")&objShare("user0,user2")&objExp("0")': PUTC_SUCCESS,     # Updates existing gdpr5
    'query(PUTC("gdpr5","VAL"))&sessionKey("user1")&objOrig("src3")&monitor("false")&objObjections("purpose3")&objPur("purpose0,purpose1")&objShare("user0")&objExp("0")': PUTC_SUCCESS,                   # Updates gdpr5 again
    
    # DELETE Tests
    'query(DELETE("gdpr4"))&sessionKey("user1")': DELETE_SUCCESS,  # Owner can delete
    'query(DELETE("gdpr1"))&sessionKey("user2")': DELETE_FAILED,  # Non-owner cannot delete
    
    # Complex Tests - Track state changes
    'query(PUT("cascade1","VAL"))&sessionKey("user1")&objPur("purpose1,purpose2")&objShare("user2")&objExp("0")': PUT_SUCCESS,               # Creates cascade1 with purpose1,2
    'query(PUTM("cascade1"))&sessionKey("user1")&objPur("purpose1,purpose2,purpose3")': PUTM_SUCCESS,                                        # Updates cascade1 to have purpose1,2,3
    'query(GET("cascade1"))&sessionKey("user2")&objPurIs("purpose3")': 'VAL',                                                      # user2 can now access with purpose3
    'query(PUT("objtest1","VAL"))&sessionKey("user1")&objPur("purpose1,purpose2")&objObjections("purpose2")&objExp("0")': PUT_SUCCESS,    # Creates objtest1, purpose2 objected
    'query(GET("objtest1"))&sessionKey("user1")&objPurIs("purpose2")': GET_FAILED,                                                       # purpose2 is objected
    'query(PUT("sharetest1","VAL"))&sessionKey("user1")&objPur("purpose1,purpose2")&objShare("user2,user3")&objExp("0")': PUT_SUCCESS,   # Creates sharetest1, shared with user2,3
    'query(GET("sharetest1"))&sessionKey("user2")&objPurIs("purpose1")': 'VAL',                                                   # user2 is shared
    'query(GET("sharetest1"))&sessionKey("user4")&objPurIs("purpose1")': GET_FAILED,                                                     # user4 not shared
    
    # Multi-User Tests
    'query(PUT("multitest1","VAL"))&sessionKey("user1")&objPur("purpose1,purpose2,purpose3")&objShare("user2")&objExp("0")': PUT_SUCCESS, # Creates multitest1
    'query(GET("multitest1"))&sessionKey("user1")&objPurIs("purpose1,purpose2")': 'VAL',                                         # Owner access
    'query(GET("multitest1"))&sessionKey("user2")&objPurIs("purpose1,purpose2")': 'VAL',                                         # Shared access
    'query(GET("multitest1"))&sessionKey("user3")&objPurIs("purpose1")': GET_FAILED,                                                    # user3 not shared
    'query(PUTM("multitest1"))&sessionKey("user1")&objPur("purpose1,purpose2,purpose4")': PUTM_SUCCESS,                                  # Updates purposes to 1,2,4
    'query(GET("multitest1"))&sessionKey("user2")&objPurIs("purpose4")': 'VAL',                                                  # user2 can access with new purpose4
    
    # Purpose Limitation Tests
    'query(PUT("purtest1","VAL"))&sessionKey("user1")&objPur("purpose1")&objObjections("purpose2,purpose3")&objExp("0")': PUT_SUCCESS,  # Only purpose1 allowed, 2,3 objected
    'query(GET("purtest1"))&sessionKey("user1")&objPurIs("purpose1")': 'VAL',                                                   # purpose1 allowed
    'query(GET("purtest1"))&sessionKey("user1")&objPurIs("purpose2")': GET_FAILED,                                                     # purpose2 objected
    'query(GET("purtest1"))&sessionKey("user1")&objPurIs("purpose1,purpose2")': GET_FAILED,                                            # Mixed allowed/objected = fail
    'query(GET("purtest1"))&sessionKey("user1")&objPurIs("purpose4")': GET_FAILED,                                                     # purpose4 not in allowed list
    
    # Bulk Operations
    'query(PUT("bulk1","VAL"))&sessionKey("user1")&objPur("purpose1,purpose2")&objShare("user2")&objExp("0")': PUT_SUCCESS,             # Creates bulk1
    'query(PUT("bulk2","VAL"))&sessionKey("user1")&objPur("purpose2,purpose3")&objShare("user3")&objExp("0")': PUT_SUCCESS,             # Creates bulk2  
    'query(PUT("bulk3","VAL"))&sessionKey("user2")&objPur("purpose1,purpose3")&objShare("user1")&objExp("0")': PUT_SUCCESS,             # Creates bulk3 (user2 owns)
    'query(GETM("bulk","data"))&sessionKey("user1")&objPurIs("purpose1,purpose2")': 'VAL',                                      # Gets bulk1 (user1 owns) + bulk3 (shared)
    'query(GETM("bulk","data"))&sessionKey("user2")&objPurIs("purpose1")': 'VAL|VAL',                                               # Gets bulk3 (user2 owns) + bulk1 (shared)
    'query(PUTM("bulk"))&sessionKey("user1")&objPur("purpose1,purpose2,purpose4")': PUTM_SUCCESS,                                       # Updates bulk1,bulk2 only (user1 owns)
    
    # Ownership Transfer (PUTC cannot transfer ownership)
    'query(PUT("transfer1","VAL"))&sessionKey("user1")&objPur("purpose1,purpose2")&objShare("user2")&objExp("0")': PUT_SUCCESS,         # Creates transfer1
    'query(PUTC("transfer1","VAL"))&sessionKey("user2")&objOrig("src2")&objPur("purpose2,purpose3")&objShare("user1")&objExp("0")': PUTC_FAILED,  # user2 cannot use PUTC (not owner)
    'query(GET("transfer1"))&sessionKey("user2")&objPurIs("purpose2")': 'VAL',                                                  # user2 still shared, can access original
    
    # Error Conditions
    'query(GET("nonexistent"))&sessionKey("user1")&objPurIs("purpose1")': GET_FAILED,          # Key doesn't exist
    'query(PUT("errortest1","VAL"))&sessionKey("user1")&objPur("purpose1")&objShare("user2")&objExp("0")': PUT_SUCCESS,  # Creates errortest1
    'query(GET("errortest1"))': GET_FAILED,                                                  # Default session (user0), uses default policy but purposes are not all included
    'query(GET("errortest1"))&sessionKey("user1")': GET_FAILED,                             # Owner access but not with proper allowed purposes
    'query(DELETE("errortest1"))&sessionKey("user2")': DELETE_FAILED,                            # user2 cannot delete (not owner)
    'query(DELETE("errortest1"))&sessionKey("user1")': DELETE_SUCCESS,                            # Owner can delete
    'query(GET("errortest1"))&sessionKey("user1")&objPurIs("purpose1")': GET_FAILED,          # Key deleted, no longer exists
    
    # Final Validation
    'query(PUT("final1","VAL"))&sessionKey("user1")&objOrig("src1")&monitor("true")&objObjections("purpose4,purpose5")&objPur("purpose0,purpose1,purpose2,purpose3")&objShare("user0,user2,user3")&objExp("0")': PUT_SUCCESS,  # Creates final1
    'query(GET("final1"))&sessionKey("user1")&objPurIs("purpose1,purpose2,purpose3")': 'VAL',  # Owner, all purposes allowed
    'query(GET("final1"))&sessionKey("user2")&objPurIs("purpose1,purpose2")': 'VAL',           # Shared, purposes allowed
    'query(GET("final1"))&sessionKey("user3")&objPurIs("purpose3")': 'VAL',                    # Shared, purpose allowed
    'query(GET("final1"))&sessionKey("user1")&objPurIs("purpose4")': GET_FAILED,                      # purpose4 objected
    'query(GET("final1"))&sessionKey("user1")&objPurIs("purpose5")': GET_FAILED,                      # purpose5 objected
    'query(GETM("final","data"))&sessionKey("user1")&objPurIs("purpose1,purpose2,purpose3")': 'VAL',  # Gets final1 data
    
    # GET_META_ONLY Tests
    'query(PUT("gdpr1","VAL"))&sessionKey("user1")&objOrig("src1")&monitor("false")&objObjections("purpose3")&objPur("purpose0,purpose1,purpose2")&objShare("user0")&objExp("0")': PUT_SUCCESS,
    'query(GET("gdpr1","meta_only"))&sessionKey("user1")&objPurIs("purpose0")': 'metadata_string',  # Returns metadata of gdpr1 (owner can access) + allowed purpose
    'query(GET("gdpr1","meta_only"))&sessionKey("user2")': GET_META_FAILED,                # user2 not authorized for gdpr1
    'query(GET("key0","meta_only"))&sessionKey("user0")': 'metadata_string',  # Returns metadata of key1 (owner)
    'query(GET("nonexistent","meta_only"))&sessionKey("user1")': GET_META_FAILED,         # Key doesn't exist

    # PUT_META_ONLY Tests
    'query(PUT("gdpr2","meta_only"))&sessionKey("user1")&objPur("purpose7,purpose8")': PUT_META_SUCCESS,  # Owner updates metadata
    'query(GET("gdpr2","meta_only"))&sessionKey("user1")&objPurIs("purpose7")': 'metadata_string',                # Metadata updated
    'query(GET("gdpr2","meta_only"))&sessionKey("user1")&objPurIs("purpose1")': GET_META_FAILED, #Check that purposes are actually updated
    'query(GET("gdpr2"))&sessionKey("user1")&objPurIs("purpose7")': 'VAL',                   # Data preserved, new purpose works
    'query(PUT("gdpr2","meta_only"))&sessionKey("user2")&objPur("purpose9,purpose10")': PUT_META_FAILED, # Non-owner cannot update
    'query(PUT("key2","meta_only"))&sessionKey("user0")&objShare("user1,user2,user3")': PUT_META_SUCCESS, # Updates sharing
    'query(GET("key2"))&sessionKey("user3")&objPurIs("purpose1,purpose2")': 'VAL',           # New shared user can access
    'query(PUT("metaonly_nokey","meta_only"))&sessionKey("user1")&objPur("purpose1,purpose2")': PUT_META_FAILED,  # Cannot update non-existent key

    # DELETEM Tests
    'query(PUT("del1","VAL"))&sessionKey("user1")&objOrig("src1")&monitor("false")&objPur("purpose1,purpose2")&objShare("user0")&objExp("0")': PUT_SUCCESS,
    'query(PUT("del2","VAL"))&sessionKey("user1")&objOrig("src1")&monitor("false")&objPur("purpose1,purpose2")&objShare("user0")&objExp("0")': PUT_SUCCESS,
    'query(PUT("del3","VAL"))&sessionKey("user2")&objOrig("src1")&monitor("false")&objPur("purpose3,purpose4")&objShare("user0")&objExp("0")': PUT_SUCCESS,
    'query(DELETEM("del"))&sessionKey("user1")&objPurIs("purpose1,purpose2")': DELETEM_SUCCESS,  # Deletes del1, del2 (2 keys)
    'query(GET("del1"))&sessionKey("user1")&objPurIs("purpose1")': GET_FAILED,  # del1 deleted
    'query(GET("del2"))&sessionKey("user1")&objPurIs("purpose1")': GET_FAILED,  # del2 deleted
    'query(GET("del3"))&sessionKey("user2")&objPurIs("purpose3")': 'VAL',  # del3 still exists

    # Ownership Tests
    'query(PUT("ownership1","VAL"))&sessionKey("user1")&objPur("purpose1,purpose2")&objExp("0")': PUT_SUCCESS,
    'query(PUT("ownership2","VAL"))&sessionKey("user1")&objPur("purpose1,purpose2")&objExp("0")': PUT_SUCCESS,
    'query(DELETEM("ownership"))&sessionKey("user2")&objPurIs("purpose1")': DELETEM_EMPTY,  # No keys deleted (user2 not owner)
    'query(GET("ownership1"))&sessionKey("user1")&objPurIs("purpose1")': 'VAL',  # Still exists
    'query(GET("ownership2"))&sessionKey("user1")&objPurIs("purpose1")': 'VAL',  # Still exists

    # Combined Operations
    'query(PUT("lifecycle1","VAL"))&sessionKey("user1")&objPur("purpose1,purpose2")&objShare("user2")&objExp("0")': PUT_SUCCESS,
    'query(PUT("lifecycle1","meta_only"))&sessionKey("user1")&objPur("purpose3,purpose4")': PUT_META_SUCCESS,
    'query(GET("lifecycle1","meta_only"))&sessionKey("user1")&objPurIs("purpose3")': 'metadata_string',
    'query(GET("lifecycle1","meta_only"))&sessionKey("user1")&objPurIs("purpose9")': GET_META_FAILED,
    'query(GET("lifecycle1","meta_only"))&sessionKey("user1")&objPurIs("purpose1")': GET_META_FAILED,
    'query(GET("lifecycle1"))&sessionKey("user2")&objPurIs("purpose3")': 'VAL', # make sure it didnt touch the other properties, e.g., share
    'query(GET("lifecycle1"))&sessionKey("user1")&objPurIs("purpose3")': 'VAL',  # New purpose works, data preserved
    'query(DELETE("lifecycle1"))&sessionKey("user1")': DELETE_SUCCESS,

    'query(PUT("batch1","VAL"))&sessionKey("user1")&objPur("purpose1")&objExp("0")': PUT_SUCCESS,
    'query(PUT("batch2","VAL"))&sessionKey("user1")&objPur("purpose1")&objExp("0")': PUT_SUCCESS,
    'query(PUT("batch1","meta_only"))&sessionKey("user1")&objPur("purpose5")': PUT_META_SUCCESS,
    'query(PUT("batch2","meta_only"))&sessionKey("user1")&objPur("purpose5")': PUT_META_SUCCESS,
    'query(DELETEM("batch"))&sessionKey("user1")&objPurIs("purpose5")': DELETEM_SUCCESS,  # Deletes both
    'query(GET("batch1"))&sessionKey("user1")&objPurIs("purpose5")': GET_FAILED,  # Deleted
    'query(GET("batch2"))&sessionKey("user1")&objPurIs("purpose5")': GET_FAILED,  # Deleted

    # Edge Cases
    'query(GET("key0","meta_only"))': 'metadata_string',  # Default metadata
    'query(PUT("gdpr3","meta_only"))&sessionKey("user5")&objPur("purpose10")': PUT_META_FAILED,  # Invalid user
    'query(GET("gdpr3","meta_only"))&sessionKey("user1")&objPurIs("purpose5,purpose6")': 'metadata_string',  # Unchanged
    'query(DELETEM("nomatch"))&sessionKey("user1")&objPurIs("purpose99")': DELETEM_EMPTY,  # No matches
    'query(PUT("edge1","VAL"))&sessionKey("user1")&objPur("purpose1")&objExp("0")': PUT_SUCCESS,
    'query(DELETEM("edge"))&sessionKey("user1")&objPurIs("purpose99")': DELETEM_EMPTY,  # No matching purposes
    'query(GET("edge1"))&sessionKey("user1")&objPurIs("purpose1")': 'VAL',  # Still exists

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
  
  # Special handling for metadata_string - accept any non-error response
  if expected == 'metadata_string':
    return 'METADATA_ACCEPTED'  # Placeholder for validation
  
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
      
      # Try to decode as UTF-8, fallback to raw bytes if it fails
      try:
        actual = response.decode('utf-8')
      except UnicodeDecodeError:
        # Binary metadata response - convert to hex string for display
        actual = response.hex()
        is_binary = True
      else:
        is_binary = False
      
      # Special handling for metadata responses
      if expected == 'METADATA_ACCEPTED':
        # For metadata queries, just check it's not an error code
        if is_binary:
          # Binary metadata is valid (not an error)
          status = "✓"
          print(f"[{status}] Binary metadata response (length: {len(response)} bytes)")
        else:
          status = "✓" if actual.strip() not in [GET_FAILED] else "✗"
          print(f"[{status}] Metadata response: {actual.strip()}")
          if status == "✗":
            print(f"[Failed {i}th query : {query[:-1]}]")
          
      else:
        # Regular comparison (only for text responses)
        if is_binary:
          status = "✗"
          print(f"[{status}] Unexpected binary response for non-metadata query")
          print(f"[Failed {i}th query : {query[:-1]}]")
        else:
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
