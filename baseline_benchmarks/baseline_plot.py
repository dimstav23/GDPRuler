import os
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

# Function to read CSV files from the "results" folder
def read_csv_files(folder_path):
  data = {}
  for filename in os.listdir(folder_path):
    if filename.endswith(".csv") and "test" not in filename:
      full_path = os.path.join(folder_path, filename)
      database, workload, num_clients = filename[:-4].split('-')
      df = pd.read_csv(full_path)
      data[(database, workload, int(num_clients))] = df
  return data

# Function to plot bar plot with latency for each workload and number of clients
def plot_latency_by_workload(data):
  databases = set(database for (database, _, _), _ in data.items())
  for database in databases:
    database_data = {key: value for key, value in data.items() if database in key}
    workloads = sorted(set(workload for (_, workload, _) in database_data.keys()))
    num_clients_dict = {}
    for key in database_data.keys():
      _, workload, _ = key
      workload_data = {num_clients: df.loc[df['Metric'] == 'Average latency (ms)', 'Value'].values[0] for (_, w, num_clients), df in database_data.items() if w == workload}
      num_clients_dict[workload] = list(workload_data.keys())
    num_clients_all = sorted(set(np.concatenate(list(num_clients_dict.values()))))
    num_workloads = len(workloads)
    width = 0.8 / num_workloads  # Width of each bar
    for i, workload in enumerate(workloads):
      x_pos = np.arange(len(num_clients_all)) + (i - num_workloads/2 + 0.5) * width
      workload_data = {num_clients: df.loc[df['Metric'] == 'Average latency (ms)', 'Value'].values[0] for (_, w, num_clients), df in database_data.items() if w == workload}
      latency_values = [workload_data.get(num_clients, 0) for num_clients in num_clients_all]
      plt.bar(x_pos, latency_values, width=width, label=workload)
    plt.xlabel('Number of Clients')
    plt.ylabel('Average Latency (ms)')
    plt.title(f'Latency by Workload ({database})')
    plt.xticks(np.arange(len(num_clients_all)), num_clients_all)
    plt.legend()
    plt.savefig(f'plots/latency_by_workload_{database}.png')
    plt.close()

# Function to plot bar plot with throughput for each workload and number of clients
def plot_throughput_by_workload(data):
  databases = set(database for (database, _, _), _ in data.items())
  for database in databases:
    database_data = {key: value for key, value in data.items() if database in key}
    workloads = sorted(set(workload for (_, workload, _) in database_data.keys()))
    num_clients_dict = {}
    for key in database_data.keys():
      _, workload, _ = key
      workload_data = {num_clients: df.loc[df['Metric'] == 'Total throughput (ops/s)', 'Value'].values[0] for (_, w, num_clients), df in database_data.items() if w == workload}
      num_clients_dict[workload] = list(workload_data.keys())
    num_clients_all = sorted(set(np.concatenate(list(num_clients_dict.values()))))
    num_workloads = len(workloads)
    width = 0.8 / num_workloads  # Width of each bar
    for i, workload in enumerate(workloads):
      x_pos = np.arange(len(num_clients_all)) + (i - num_workloads/2 + 0.5) * width
      workload_data = {num_clients: df.loc[df['Metric'] == 'Total throughput (ops/s)', 'Value'].values[0] for (_, w, num_clients), df in database_data.items() if w == workload}
      throughput_values = [workload_data.get(num_clients, 0) for num_clients in num_clients_all]
      plt.bar(x_pos, throughput_values, width=width, label=workload)
    plt.xlabel('Number of Clients')
    plt.ylabel('Total Throughput (ops/s)')
    plt.title(f'Throughput by Workload ({database})')
    plt.xticks(np.arange(len(num_clients_all)), num_clients_all)
    plt.legend()
    plt.savefig(f'plots/throughput_by_workload_{database}.png')
    plt.close()

# Function to plot bar plot with latency for all databases and workloads
def plot_combined_latency(data):
  all_workloads = sorted(set(workload for (_, workload, _) in data.keys()))
  all_databases = sorted(set(database for (database, _, _) in data.keys()))
  num_clients_dict = {}
  for key in data.keys():
    _, _, num_clients = key
    num_clients_dict[num_clients] = 1
  num_clients_all = sorted(num_clients_dict.keys())
  width = 0.8 / (len(all_workloads) * len(all_databases))  # Width of each bar
  for database in all_databases:
    for i, workload in enumerate(all_workloads):
      x_pos = np.arange(len(num_clients_all)) + (i + len(all_workloads) * all_databases.index(database)) * width
      latency_values = []
      for num_clients in num_clients_all:
        key = (database, workload, num_clients)
        if key in data:
          latency_values.append(data[key].loc[data[key]['Metric'] == 'Average latency (ms)', 'Value'].values[0])
        else:
          latency_values.append(0)
      plt.bar(x_pos, latency_values, width=width, label=f'{database}-{workload}')
  plt.xlabel('Number of Clients')
  plt.ylabel('Average Latency (ms)')
  plt.title('Combined Latency by Workload and Database')
  plt.xticks(np.arange(len(num_clients_all)) + 0.4, num_clients_all)
  plt.legend()
  plt.tight_layout()
  plt.savefig('plots/combined_latency.png')
  plt.close()

# Function to plot bar plot with throughput for all databases and workloads
def plot_combined_throughput(data):
  all_workloads = sorted(set(workload for (_, workload, _) in data.keys()))
  all_databases = sorted(set(database for (database, _, _) in data.keys()))
  num_clients_dict = {}
  for key in data.keys():
    _, _, num_clients = key
    num_clients_dict[num_clients] = 1
  num_clients_all = sorted(num_clients_dict.keys())
  width = 0.8 / (len(all_workloads) * len(all_databases))  # Width of each bar
  for database in all_databases:
    for i, workload in enumerate(all_workloads):
      x_pos = np.arange(len(num_clients_all)) + (i + len(all_workloads) * all_databases.index(database)) * width
      throughput_values = []
      for num_clients in num_clients_all:
        key = (database, workload, num_clients)
        if key in data:
          throughput_values.append(data[key].loc[data[key]['Metric'] == 'Total throughput (ops/s)', 'Value'].values[0])
        else:
          throughput_values.append(0)
      plt.bar(x_pos, throughput_values, width=width, label=f'{database}-{workload}')
  plt.xlabel('Number of Clients')
  plt.ylabel('Total Throughput (ops/s)')
  plt.title('Combined Throughput by Workload and Database')
  plt.xticks(np.arange(len(num_clients_all)) + 0.4, num_clients_all)
  plt.legend()
  plt.tight_layout()
  plt.savefig('plots/combined_throughput.png')
  plt.close()

# Main function to execute all plotting functions
def main():
  folder_path = 'results'
  plots_path = 'plots'
  # Create the "plots" folder if it doesn't exist
  os.makedirs(plots_path, exist_ok=True)
  
  data = read_csv_files(folder_path)
  plot_latency_by_workload(data)
  plot_throughput_by_workload(data)
  plot_combined_latency(data)
  plot_combined_throughput(data)

# Call the main function
if __name__ == "__main__":
  main()
