import time

from enum import Enum

from policy_compiler.helper import safe_open
from policy_compiler.query_analyser import analyze_query

class DbType(str, Enum):
  """GDPRuler db type."""

  ROCKSDB = "rocksdb"
  REDIS = "redis"

def execute_queries(controller, workload_file):
  # Start the time measurement before sending the workload
  start_time = time.perf_counter_ns()

  # Parse and send the query args to the native controller
  workload_file = safe_open(workload_file, "r")
  queries = workload_file.readlines()

  for query in queries:
    if query.startswith("#"):
      continue  # Skip queries starting with '#'
    new_query = analyze_query(query.rstrip())
    controller.stdin.write(new_query.encode() + b'\n')
    controller.stdin.flush()

  controller.stdin.write(b'exit\n')
  controller.stdin.flush()

  # Read process' standard output and error
  _, error = controller.communicate()
  controller.terminate()

  # End the timer after the controller has returned and analyse the measurements
  end_time = time.perf_counter_ns()
  runtime = end_time - start_time

  # Print the output of the controller process
  if error:
    print(error.decode('utf-8'))

  # Print the timer results
  seconds = runtime / 1000000000
  print("System time: {:.9f} s".format(seconds))
