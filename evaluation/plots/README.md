# Bare metal results

The [`bare_metal_plots.py`](./bare_metal_plots.py) script generates latency and throughput bar plots from CSV data files containing performance metrics of various database controllers and workloads. 
It helps visualize the performance characteristics under different configurations, including encryption and logging settings.

## Table of Contents
- [Requirements](#requirements)
- [Input Data Structure](#input-data-structure)
- [Usage](#usage)
- [Output](#output)

## Requirements

- Python 3.x
- `pandas`
- `matplotlib`
- `seaborn`

You can install the required libraries using pip:

```bash
pip install pandas matplotlib seaborn
```

## Input Data Structure

The script expects a directory containing CSV files that adhere to the following naming convention:

```
<controller>-<workload_type>_<ops>-encryption_<encryption>-logging_<logging>.csv
```

**Example:**
- `gdpr_ctl-query_mgmt_mediume-encryption_OFF-logging_ON.csv`
- `native_ctl-query_mgmt_medium-encryption_ON-logging_ON.csv`

Each CSV file should have the following columns:
- `elapsed_time (s)`: The time taken to complete the operations.
- The first column should contain workload names starting with "workload" (e.g., `workloada`, `workloadb`, etc.).

## Usage

To run the script, navigate to the directory containing the script in your terminal and execute the following command:

```bash
python3 generate_plots.py --bare_metal_results <path_to_bare_metal_results> --vm_results <path_to_vm_results> --output_dir <path_to_output_directory>
```

- `--bare_metal_results`: (Optional) Directory containing the bare-metal result CSV files. Default is `../bare_metal/results`.
- `--vm_results`: (Optional) Directory containing the VM result CSV files. Default is `../VM/results`.
- `--output_dir`: (Optional) Directory to save the generated plots. Default is `plots`.

## Output

The script generates a set of plots for all the metrics (latency, throughput) DBs (redis, rocksdb) and all the variants and places them in the output directory.