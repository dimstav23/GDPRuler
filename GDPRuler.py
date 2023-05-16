import json
import argparse
import subprocess
import os

from common import execute_queries, DbType
from policy_compiler.helper import safe_open
from policy_compiler.policy_config import parse_user_policy

# NOTE: we use send(str+"\n") to communicate with the process because the sendline() hangs with 0 delaybeforesend

# Define a custom validation function for the --db_encryptionkey argument
def validate_encryption_key(key):
  if len(key) != 16:
    raise argparse.ArgumentTypeError('Encryption keys must be exactly 16 characters long')
  return key

def main():
  default_db_encryption_key = "0123456789abcdef"
  default_log_encryption_key = "abcdef0123456789"
  parser = argparse.ArgumentParser(description='Start GDPRuler instance for a specific workload.')
  parser.add_argument('--config', help='path to the default config trace file', default=None, required=True, type=str)
  parser.add_argument('--workload', help='path to the workload trace file', default=None, required=True, type=str)
  parser.add_argument('--db', help='db to use, one of {rocksdb,redis}', default=DbType.ROCKSDB, required=False, type=DbType)
  parser.add_argument('--address', help='db ip address for client to connect', default=None, required=False, type=str)
  parser.add_argument('--logpath', help='folder to place the gdpr log files', default="./logs", required=False, type=str)
  parser.add_argument('--db_encryptionkey', help='DB encryption/decryption key. Expected to be exactly 16 chars', 
                      default=default_db_encryption_key, required=False, type=validate_encryption_key)
  parser.add_argument('--log_encryptionkey', help='Log encryption/decryption key. Expected to be exactly 16 chars', 
                      default=default_log_encryption_key, required=False, type=validate_encryption_key)
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
  if args.db_encryptionkey:
    process_args += ['--db_encryptionkey', args.db_encryptionkey]
  if args.log_encryptionkey:
    process_args += ['--log_encryptionkey', args.log_encryptionkey]
  controller = subprocess.Popen(process_args, stdin=subprocess.PIPE, stderr=subprocess.PIPE)

  # Write policy to process' standard input
  controller.stdin.write(def_policy.encode() + b'\n')
  controller.stdin.flush()

  # Execute queries
  execute_queries(controller, args.workload)

  return

if __name__ == "__main__":
  main()