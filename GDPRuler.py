import json
import argparse
import subprocess
import os

from common import execute_queries, DbType
from policy_compiler.helper import safe_open
from policy_compiler.policy_config import parse_user_policy

# NOTE: we use send(str+"\n") to communicate with the process because the sendline() hangs with 0 delaybeforesend

def main():
  default_cryption_key = "0123456789abcdefghjk0123456789yz"
  parser = argparse.ArgumentParser(description='Start GDPRuler instance for a specific workload.')
  parser.add_argument('--config', help='path to the default config trace file', default=None, required=True, type=str)
  parser.add_argument('--workload', help='path to the workload trace file', default=None, required=True, type=str)
  parser.add_argument('--db', help='db to use, one of {rocksdb,redis}', default=DbType.ROCKSDB, required=False, type=DbType)
  parser.add_argument('--address', help='db ip address for client to connect', default=None, required=False, type=str)
  parser.add_argument('--logpath', help='folder to place the gdpr log files', default="./logs", required=False, type=str)
  parser.add_argument('--cryptionkey', help='encryption/decryption key. Expected to be exactly 32 chars', default=default_cryption_key, required=False, type=str)
  args = parser.parse_args()

  user_policy = safe_open(args.config, "r") # open the file containing the default user configuration
  user_policy = json.load(user_policy)

  # Set up the default policy in the gdpr controller
  def_policy = parse_user_policy(user_policy)

  # Open the controller process
  process_args = [os.path.join(os.path.dirname(os.path.abspath(__file__)), './controller/build/gdpr_controller')]
  process_args += ['--db', args.db]
  if args.address:
    process_args += ['--address', args.address]
  process_args += ['--logpath', args.logpath]
  if args.cryptionkey:
    process_args += ['--cryptionkey', args.cryptionkey]
  controller = subprocess.Popen(process_args, stdin=subprocess.PIPE, stderr=subprocess.PIPE)

  # Write policy to process' standard input
  controller.stdin.write(def_policy.encode() + b'\n')
  controller.stdin.flush()

  # Execute queries
  execute_queries(controller, args.workload)

  return

if __name__ == "__main__":
  main()