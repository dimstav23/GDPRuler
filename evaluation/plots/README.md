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

You can install the required libraries using pip:

```bash
pip install pandas matplotlib
```

## Input Data Structure

The script expects a directory containing CSV files that adhere to the following naming convention:

```
<controller>-<workload_type>_<ops>-encryption_<encryption>-logging_<logging>.csv
```

**Example:**
- `gdpr-query_mgmt_1M-encryption_OFF-logging_ON.csv`
- `native-query_mgmt_1M-encryption_ON-logging_ON.csv`

Each CSV file should have the following columns:
- `elapsed_time (s)`: The time taken to complete the operations.
- The first column should contain workload names starting with "workload" (e.g., `workloada`, `workloadb`, etc.).

## Usage

To run the script, navigate to the directory containing the script in your terminal and execute the following command:

```bash
python bare_metal_plots.py --input_dir <path_to_input_directory> --output_dir <path_to_output_directory>
```

- `--input_dir`: (Optional) Directory containing the CSV files. Default is `../bare_metal/results`.
- `--output_dir`: (Optional) Directory to save the generated plots. Default is `plots`.

**Example:**

```bash
python bare_metal_plots.py --input_dir ../bare_metal/results --output_dir plots
```

## Output

The script generates two types of plots:
- **Latency Plot**: Visualizes average latency per workload and controller configuration.
- **Throughput Plot**: Visualizes throughput (operations per second) per workload and controller configuration.

The plots are saved in both PNG and PDF formats in the specified output directory.


For instance, given CSV files in the `../bare_metal/results` directory, running the script will produce plots named according to the database, encryption, and logging settings, such as:

- `db_encryption_OFF_logging_ON.png`
- `db_encryption_OFF_logging_ON.pdf`