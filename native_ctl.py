import sys
import json
import argparse
import subprocess
import pexpect
import time

sys.path.insert(1, './policy_compiler') # add the policy compiler in sys path
from helper import safe_open
from query_analyser import analyze_query

# NOTE: we use send(str+"\n") to communicate with the process because the sendline() hangs with 0 delaybeforesend

def main():
  parser = argparse.ArgumentParser(description='Start Native controller instance for a specific workload.')
  parser.add_argument('-w', '--workload', help='path to the workload trace file', 
                        dest='workload', default=None, nargs=1, required=True, type=str)
  args = parser.parse_args(sys.argv[1:]) # to exclude the script name

  workload_file = args.workload[0] # the file containing the queries to test
  if "gdpr" in workload_file:
    print("please provide a non-gdpr workload for the native client")
    return
  
  # Open the controller process
  controller = subprocess.Popen(["./controller/build/native_controller"], stdin=subprocess.PIPE, 
                                                                        stdout=subprocess.PIPE, 
                                                                        stderr=subprocess.PIPE)
  # Start the time measurement before sending the workload
  start_time = time.perf_counter_ns()

  # Parse and send the query args to the native controller
  workload_file = safe_open(workload_file, "r")
  queries = workload_file.readlines()

  for query in queries:
    new_query = analyze_query(query.rstrip())
    controller.stdin.write(new_query.encode() + b'\n')
    controller.stdin.flush()

  controller.stdin.write(b'exit\n')
  controller.stdin.flush()

  # Read process' standard output and error
  output, error = controller.communicate()
  
  # End the timer after the controller has returned and analyse the measurements
  end_time = time.perf_counter_ns()
  runtime = end_time - start_time
  seconds = runtime // 1000000000
  milliseconds = (runtime // 1000000) % 1000
  nanoseconds = runtime % 1000000

  # Print the output of the controller process
  print(output.decode('utf-8'))
  print(error.decode('utf-8'))

  # Print the timer results
  print("System runtime: %d s %d ms %d ns" % (seconds, milliseconds, nanoseconds))

  return

if __name__ == "__main__":
  main()