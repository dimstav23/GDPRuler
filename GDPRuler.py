import sys
import json
import argparse
import subprocess
import pexpect
import time

sys.path.insert(1, './policy_compiler') # add the policy compiler in sys path
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

  ## open the controller process
  # controller = subprocess.Popen(["./controller/build/gdpr_controller"], stdin=subprocess.PIPE, stdout=subprocess.PIPE)
  controller = pexpect.spawn("./controller/build/gdpr_controller", echo=False)
  controller.delaybeforesend = None

  ## set up the default policy in the gdpr controller
  def_policy = parse_user_policy(user_policy)

  # write to process' standard input
  controller.send(def_policy+"\n")
  # controller.expect("default_policy:OK")

  ## parse and send the query args to the gdpr controller 
  workload_file = safe_open(workload_file, "r")
  queries = workload_file.readlines()
    
  for query in queries:
    new_query = analyze_query(query.rstrip())
    controller.send(new_query+"\n")

  controller.sendline("exit")
  output = controller.read()
  print(output.decode('utf-8'))
  controller.close()
  return

if __name__ == "__main__":
  main()