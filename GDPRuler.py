import sys
import json
import argparse
import subprocess
import pexpect
import time
import os

script_dir = os.path.dirname(os.path.abspath(__file__))
policy_compiler_dir = os.path.join(script_dir, './policy_compiler')
sys.path.insert(1, policy_compiler_dir) # add the policy compiler in sys path
from helper import safe_open
from query_analyser import analyze_query
from policy_config import parse_user_policy

# NOTE: we use send(str+"\n") to communicate with the process because the sendline() hangs with 0 delaybeforesend

def main():
  parser = argparse.ArgumentParser(description='Start GDPRuler instance for a specific workload.')
  parser.add_argument('-c', '--config', help='path to the default config trace file', 
                        dest='config', default=None, nargs=1, required=True, type=str)
  parser.add_argument('-w', '--workload', help='path to the workload trace file', 
                        dest='workload', default=None, nargs=1, required=True, type=str)
  args = parser.parse_args(sys.argv[1:]) # to exclude the script name

  config_file = args.config[0] # the file containing the default user configuration
  workload_file = args.workload[0] # the file containing the queries to test

  user_policy = safe_open(config_file, "r")
  user_policy = json.load(user_policy)

  # Open the controller process
  exec_file = os.path.join(script_dir, './controller/build/gdpr_controller')
  controller = subprocess.Popen([exec_file], stdin=subprocess.PIPE, 
                                            stdout=subprocess.PIPE, 
                                            stderr=subprocess.PIPE)

  # Start the time measurement before sending the default policy
  start_time = time.perf_counter_ns()

  # Set up the default policy in the gdpr controller
  def_policy = parse_user_policy(user_policy)

  # Write to process' standard input
  controller.stdin.write(def_policy.encode() + b'\n')
  controller.stdin.flush()

  # Parse and send the query args to the gdpr controller
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
  controller.terminate()

  # End the timer after the controller has returned and analyse the measurements
  end_time = time.perf_counter_ns()
  runtime = end_time - start_time

  # Print the output of the controller process
  if error:
    print(error.decode('utf-8'))
  print(output.decode('utf-8'), end = '')

  # Print the timer results
  seconds = runtime / 1000000000
  print("System time: {:.9f} s".format(seconds))

  return

if __name__ == "__main__":
  main()