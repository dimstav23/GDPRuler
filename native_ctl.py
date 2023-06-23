import argparse
import subprocess
import os

from common import DbType


# NOTE: we use send(str+"\n") to communicate with the process because the sendline() hangs with 0 delaybeforesend

def main():
  parser = argparse.ArgumentParser(description='Start Native controller instance.')
  parser.add_argument('--db', help='db to use, one of {rocksdb,redis}', default=DbType.ROCKSDB, required=False, type=DbType)
  parser.add_argument('--db_address', help='db IP address for client to connect', default=None, required=False, type=str)
  parser.add_argument('--frontend_address', help='frontend IP address', default="127.0.0.1", required=False, type=str)
  parser.add_argument('--frontend_port', help='frontend port', default="1312", required=False, type=str)
  args = parser.parse_args()

  # Open the controller process
  process_args = [os.path.join(os.path.dirname(os.path.abspath(__file__)), './controller/build/native_controller')]
  process_args += ['--db', args.db]
  if args.db_address:
    process_args += ['--db_address', args.db_address]
  process_args += ['--frontend_address', args.frontend_address]
  process_args += ['--frontend_port', args.frontend_port]
  controller = subprocess.Popen(process_args, stdin=subprocess.PIPE, stderr=subprocess.PIPE)
  
  # Wait for the controller process to exit
  controller.wait()
  
  return

if __name__ == "__main__":
  main()