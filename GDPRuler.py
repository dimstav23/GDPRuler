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
  parser = argparse.ArgumentParser(description='Start GDPRuler instance.')
  parser.add_argument('--config', help='path to the default config trace file', default=None, required=True, type=str)
  parser.add_argument('--db', help='db to use, one of {rocksdb,redis}', default=DbType.ROCKSDB, required=False, type=DbType)
  parser.add_argument('--db_address', help='db ip address for client to connect', default=None, required=False, type=str)
  parser.add_argument('--logpath', help='folder to place the gdpr log files', default="./logs", required=False, type=str)
  parser.add_argument('--db_encryptionkey', help='DB encryption/decryption key. Expected to be exactly 16 chars', 
                      default=default_db_encryption_key, required=False, type=validate_encryption_key)
  parser.add_argument('--log_encryptionkey', help='Log encryption/decryption key. Expected to be exactly 16 chars', 
                      default=default_log_encryption_key, required=False, type=validate_encryption_key)
  parser.add_argument('--controller_address', help='controller IP address', default="127.0.0.1", required=False, type=str)
  parser.add_argument('--controller_port', help='controller port', default="1312", required=False, type=str)
  args = parser.parse_args()

  user_policy = safe_open(args.config, "r") # open the file containing the default user configuration
  user_policy = json.load(user_policy)

  # Set up the default policy in the gdpr controller
  def_policy = parse_user_policy(user_policy)

  # Open the controller process
  process_args = [os.path.join(os.path.dirname(os.path.abspath(__file__)), './controller/build/gdpr_controller')]
  process_args += ['--db', args.db]
  if args.db_address:
    process_args += ['--db_address', args.db_address]
  process_args += ['--logpath', args.logpath]
  if args.db_encryptionkey:
    process_args += ['--db_encryptionkey', args.db_encryptionkey]
  if args.log_encryptionkey:
    process_args += ['--log_encryptionkey', args.log_encryptionkey]
  process_args += ['--controller_address', args.controller_address]
  process_args += ['--controller_port', args.controller_port]
  controller = subprocess.Popen(process_args, stdin=subprocess.PIPE, stderr=subprocess.PIPE)

  # Write policy to process' standard input
  controller.stdin.write(def_policy.encode() + b'\n')
  controller.stdin.flush()

  # Wait for the controller process to exit
  controller.wait()

  return

if __name__ == "__main__":
  main()