import argparse
import subprocess
import os

from common import execute_queries, DbType


# NOTE: we use send(str+"\n") to communicate with the process because the sendline() hangs with 0 delaybeforesend

def main():
  parser = argparse.ArgumentParser(description='Start Native controller instance for a specific workload.')
  parser.add_argument('--workload', help='path to the workload trace file', default=None, required=True, type=str)
  parser.add_argument('--db', help='db to use, one of {rocksdb,redis}', default=DbType.ROCKSDB, required=False, type=DbType)
  parser.add_argument('--address', help='db ip address for client to connect', default=None, required=False, type=str)
  args = parser.parse_args()

  if "gdpr" in args.workload:
    print("please provide a non-gdpr workload for the native client")
    return
  
  # Open the controller process
  process_args = [os.path.join(os.path.dirname(os.path.abspath(__file__)), './controller/build/native_controller')]
  process_args += ['--db', args.db]
  if args.address:
    process_args += ['--address', args.address]
  controller = subprocess.Popen(process_args, stdin=subprocess.PIPE, stderr=subprocess.PIPE)
  
  # Execute queries
  execute_queries(controller, args.workload)

  return

if __name__ == "__main__":
  main()