import json
import argparse
import subprocess
import os
import sys
from common import DbType

curr_dir = os.path.dirname(os.path.abspath(__file__))
parent_dir = os.path.dirname(curr_dir)
sys.path.insert(0, parent_dir)

# Define a custom validation function for the --db_encryptionkey argument
def validate_encryption_key(key):
  if len(key) != 16:
    raise argparse.ArgumentTypeError('Encryption keys must be exactly 16 characters long')
  return key

def main():
  default_db_encryption_key = "0123456789abcdef"
  default_log_encryption_key = "abcdef0123456789"
  parser = argparse.ArgumentParser(description='Start GDPRuler instance.')
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

  # Open the controller process
  process_args = [os.path.join(os.path.dirname(os.path.abspath(__file__)), '../controller/build/gdpr_controller')]
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

  # Wait for the controller process to exit
  controller.wait()
  
  # Read the stderr output
  stderr_output = controller.stderr.read()
  # Decode the output if it is in bytes format
  if isinstance(stderr_output, bytes):
    stderr_output = stderr_output.decode()
  # Print the stderr output if it is not empty
  if stderr_output:
    print(stderr_output)

  return

if __name__ == "__main__":
  main()