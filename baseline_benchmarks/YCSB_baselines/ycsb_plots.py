import os
import re
import matplotlib.pyplot as plt
import numpy as np

ycsb_workloads = ["workloada", "workloadb", "workloadc", "workloadd", "workloadf"]
gdpr_workloads = ["gdpr_controller", "gdpr_customer", "gdpr_processor"]

def extract_metrics(filepath):
  with open(filepath, 'r') as file:
    lines = file.readlines()
    runtime = float(re.findall(r'RunTime\(ms\), (\d+\.?\d*)', lines[0])[0])
    throughput = float(re.findall(r'Throughput\(ops/sec\), (\d+\.?\d*)', lines[1])[0])
  return runtime, throughput

# Function to read data from the result files
def read_data(directory):
  ycsb_runtime = []
  ycsb_throughput = []
  gdprbench_runtime = []
  gdprbench_throughput = []
  gdpr_workloads_runtime = []
  gdpr_workloads_throughput = []

  # read YCSB data
  for workload in ycsb_workloads:
    filename = "ycsb_" + workload + "_res"
    filepath = os.path.join(directory, filename)
    if not os.path.exists(filepath):
      print(f"File {filepath} does not exist.")
      exit()
    runtime, throughput = extract_metrics(filepath)
    ycsb_runtime.append(runtime)
    ycsb_throughput.append(throughput)
  
  # read GDPRbench data
  for workload in ycsb_workloads:
    filename = "gdprbench_" + workload + "_res"
    filepath = os.path.join(directory, filename)
    if not os.path.exists(filepath):
      print(f"File {filepath} does not exist.")
      exit()
    runtime, throughput = extract_metrics(filepath)
    gdprbench_runtime.append(runtime)
    gdprbench_throughput.append(throughput)

  for workload in gdpr_workloads:
    filename = "gdprbench_" + workload + "_res"
    filepath = os.path.join(directory, filename)
    if not os.path.exists(filepath):
      print(f"File {filepath} does not exist.")
      exit()
    runtime, throughput = extract_metrics(filepath)
    gdpr_workloads_runtime.append(runtime)
    gdpr_workloads_throughput.append(throughput)

  return ycsb_runtime, ycsb_throughput, gdprbench_runtime, gdprbench_throughput, gdpr_workloads_runtime, gdpr_workloads_throughput

# Determine the script directory
script_directory = os.path.dirname(os.path.realpath(__file__))

# Read data from result files in the "results"
results_directory = os.path.join(script_directory, "results")
ycsb_runtime, ycsb_throughput, gdprbench_runtime, gdprbench_throughput, gdpr_workloads_runtime, gdpr_workloads_throughput = read_data(results_directory)

plots_directory = os.path.join(script_directory, "plots")
if not os.path.exists(plots_directory):
  os.makedirs(plots_directory)

# Plot 1 - Bar plot for runtime
fig, ax = plt.subplots()
bar_width = 0.35
index = np.arange(len(ycsb_workloads))

bar1 = ax.bar(index, ycsb_runtime, bar_width, label='YCSB')
bar2 = ax.bar(index + bar_width, gdprbench_runtime, bar_width, label='GDPRBench')

ax.set_xlabel('YCSB Workloads')
ax.set_ylabel('Runtime (ms)')
ax.set_title('Runtime Comparison between YCSB and GDPRBench')
ax.set_xticks(index + bar_width / 2)
ax.set_xticklabels(ycsb_workloads)
ax.legend()

plt.savefig(os.path.join(plots_directory, 'runtime_comparison.png'), format = 'png')
plt.savefig(os.path.join(plots_directory, 'runtime_comparison.pdf'), format = 'pdf', dpi=300)
plt.close()

# Plot 2 - Bar plot for throughput
fig, ax = plt.subplots()
bar_width = 0.35
index = np.arange(len(ycsb_workloads))

bar1 = ax.bar(index, ycsb_throughput, bar_width, label='YCSB')
bar2 = ax.bar(index + bar_width, gdprbench_throughput, bar_width, label='GDPRBench')

ax.set_xlabel('YCSB Workloads')
ax.set_ylabel('Throughput (ops/sec)')
ax.set_title('Throughput Comparison between YCSB and GDPRBench')
ax.set_xticks(index + bar_width / 2)
ax.set_xticklabels(ycsb_workloads)
ax.legend()

plt.savefig(os.path.join(plots_directory, 'throughput_comparison.png'), format = 'png')
plt.savefig(os.path.join(plots_directory, 'throughput_comparison.pdf'), format = 'pdf', dpi=300)
plt.close()

# Plot 3 - Bar plot for GDPR workloads runtime
fig, ax = plt.subplots()

ax.bar(gdpr_workloads, gdpr_workloads_runtime)
ax.set_xlabel('GDPR Workloads')
ax.set_ylabel('Runtime (ms)')
ax.set_title('Runtime of GDPR Workloads')

plt.savefig(os.path.join(plots_directory, 'gdpr_runtime.png'), format = 'png')
plt.savefig(os.path.join(plots_directory, 'gdpr_runtime.pdf'), format = 'pdf', dpi=300)
plt.close()

# Plot 4 - Bar plot for GDPR workloads throughput
fig, ax = plt.subplots()

ax.bar(gdpr_workloads, gdpr_workloads_throughput)
ax.set_xlabel('GDPR Workloads')
ax.set_ylabel('Throughput (ops/sec)')
ax.set_title('Throughput of GDPR Workloads')

plt.savefig(os.path.join(plots_directory, 'gdpr_throughput.png'), format = 'png')
plt.savefig(os.path.join(plots_directory, 'gdpr_throughput.pdf'), format = 'pdf', dpi=300)
plt.close()
