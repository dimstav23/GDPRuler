import socket
import argparse
import multiprocessing
import time
from policy_compiler.helper import safe_open

exit_query="query(exit)\n"

def send_queries(server_address, server_port, workload_file, latency_results):
  # Open a connection to the server
  client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
  client_socket.connect((server_address, server_port))

  # Read the contents of the workload file line by line
  total_latency = 0
  request_count = 0
  with safe_open(workload_file, 'r') as file:
    for line in file:
      if line.startswith("#"):
        continue  # Skip queries starting with '#'
      # Send each line to the server
      start_time = time.perf_counter()  # Start the timer
      client_socket.send(line.encode())

      # Receive the server's response
      response = client_socket.recv(1024).decode().strip()
      # print(response)
      end_time = time.perf_counter()  # End the timer

      # Calculate and accumulate the latency
      latency = end_time - start_time
      total_latency += latency
      request_count += 1
      # print(request_count)
  
  client_socket.send(exit_query.encode())
  # Close the connection
  client_socket.close()

  # Save the average latency
  if request_count > 0:
    average_latency = total_latency / request_count
    latency_results.append(average_latency)

def create_client_process(server_address, server_port, workload_file, latency_results):
  process = multiprocessing.Process(target=send_queries, args=(server_address, server_port, workload_file, latency_results))
  process.start()
  return process

def main():
  parser = argparse.ArgumentParser(description='Start a client.')
  parser.add_argument('--workload', help='Path to the workload trace file', default=None, required=True, type=str)
  parser.add_argument('--address', help='IP address of the server to connect', default="127.0.0.1", required=False, type=str)
  parser.add_argument('--port', help='Port of the running server to connect', default=1312, required=False, type=int)
  parser.add_argument('--clients', help='Number of clients to spawn', default=1, type=int)
  args = parser.parse_args()

  # Start the time measurement before sending the workload
  start_time = time.perf_counter()

  manager = multiprocessing.Manager()
  latency_results = manager.list()
  processes = []
  for _ in range(args.clients):
    process = create_client_process(args.address, args.port, args.workload, latency_results)
    processes.append(process)

  # Wait for all client processes to finish
  for process in processes:
    process.join()

  # End the timer after the controller has returned
  end_time = time.perf_counter()
  elapsed_time = end_time - start_time

  # Calculate and print the average latency
  if len(latency_results) > 0:
    average_latency = sum(latency_results) / len(latency_results)
    print(f"Average Latency: {average_latency:.6f} seconds")
  else:
    print("Did not gathered latency statistics --- experiments failed.")

  # Print the overall elapsed time
  print(f"Elapsed time: {elapsed_time:.3f} seconds")

if __name__ == "__main__":
  main()
