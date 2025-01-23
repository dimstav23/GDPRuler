import subprocess
import argparse
import multiprocessing
import time
import os
import sys

curr_dir = os.path.dirname(os.path.abspath(__file__))
parent_dir = os.path.dirname(curr_dir)
sys.path.insert(0, parent_dir) 
from policy_compiler.helper import safe_open

def send_queries(db_type, db_address, workload_file, latency_results):
    # Open the client process
    process_args = [os.path.join(os.path.dirname(os.path.abspath(__file__)), '../controller/build/direct_kv_client')]
    process_args += ['--db', db_type]
    process_args += ['--db_address', db_address]

    client_process = subprocess.Popen(process_args, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    
    total_latency = 0
    request_count = 0

    with safe_open(workload_file, 'r') as file:
        for line in file:
            if line.startswith("#"):
                continue  # Skip queries starting with '#'
            
            start_time = time.perf_counter()  # Start the timer

            # Send the query to the client process
            client_process.stdin.write(line.encode())
            client_process.stdin.flush()

            # Read the response from the client process
            response = client_process.stdout.readline().strip()

            end_time = time.perf_counter()  # End the timer

            # Calculate and accumulate the latency
            latency = end_time - start_time
            total_latency += latency
            request_count += 1

    # Close the client process
    client_process.terminate()
    client_process.wait()

    # Save the average latency
    if request_count > 0:
        average_latency = total_latency / request_count
        latency_results.append(average_latency)

def create_client_process(db_type, db_address, workload_file, latency_results):
    process = multiprocessing.Process(target=send_queries, args=(db_type, db_address, workload_file, latency_results))
    process.start()
    return process

def main():
    parser = argparse.ArgumentParser(description='Start a client.')
    parser.add_argument('--workload', help='Path to the workload trace file', required=True, type=str)
    parser.add_argument('--db', help='Database type (redis or rocksdb)', required=True, type=str)
    parser.add_argument('--db_address', help='Address of the database server', required=True, type=str)
    parser.add_argument('--clients', help='Number of clients to spawn', default=1, type=int)
    args = parser.parse_args()

    # Start the time measurement before sending the workload
    start_time = time.perf_counter()

    manager = multiprocessing.Manager()
    latency_results = manager.list()
    processes = []
    for _ in range(args.clients):
        process = create_client_process(args.db, args.db_address, args.workload, latency_results)
        processes.append(process)

    # Wait for all client processes to finish
    for process in processes:
        process.join()

    # End the timer after all clients have finished
    end_time = time.perf_counter()
    elapsed_time = end_time - start_time

    # Calculate and print the average latency
    if len(latency_results) > 0:
        average_latency = sum(latency_results) / len(latency_results)
        print(f"Average Latency: {average_latency:.6f} seconds")
    else:
        print("Did not gather latency statistics --- experiment failed.")

    # Print the overall elapsed time
    print(f"Elapsed time: {elapsed_time:.3f} seconds")

if __name__ == "__main__":
    main()
