# YCSB Performance Analysis Scripts

Two Python scripts that analyze and visualize YCSB database performance data from bare metal and CVM environments.

## Scripts

### `generate_plots.py`
Creates detailed individual plots for comprehensive analysis.
- Separate plots for each database (`Redis`, `RocksDB`)
- All workloads (A, B, C, D, F) and thread counts (1, 2, 4, 8, 16)
- Both latency and throughput metrics
- ~40+ plots total

### `generate_ycsb_performance_plot.py`
Creates summary plots (for the paper).
- 2×3 grid layout (`Redis`/`RocksDB` rows)
- Key scenarios: workload comparison, scaling analysis
- 2 plots total

### `generate_logging_plot_and_stats.py`
Analyzes logging impact on database performance.
- `logging=ON` data only
- Different logging percentages (e.g., 0%, 10%, 50%, 100%)
- Side-by-side Redis/RocksDB throughput comparison
- Storage overhead analysis with percentage differences

## Requirements
The required packages are already include in the nix development environment.
```
pandas matplotlib seaborn numpy
```

## Usage

### Basic Performance Analysis

**Use default directories**
```
python3 generate_plots.py
python3 generate_ycsb_performance_plot.py
```

**Specify custom directories**
```
python3 generate_plots.py --bare_metal_results <path> --vm_results <path> --output_dir <path>
```

**Default paths:**
- Bare metal results: `../bare_metal/results`
- VM results: `../VM/results`
- Output: `plots/`

### Logging Impact Analysis
**Default configuration (gdpr_CVM, 8 clients, encryption ON)**
```
python3 generate_logging_plot_and_stats.py
```

**Custom configuration**
```
python3 generate_logging_plot_and_stats.py
--variant gdpr_bare_metal
--n_clients 16
--encryption OFF
--output_dir custom_plots
```

**Available parameters:**
- `--variant`: `gdpr_bare_metal` or `gdpr_CVM` (default: `gdpr_CVM`)
- `--n_clients`: Thread count to analyze (default: `8`)
- `--encryption`: `ON` or `OFF` (default: `ON`)
- `--bare_metal_results`: Path to bare metal results (default: `../bare_metal/results`)
- `--vm_results`: Path to VM results (default: `../VM/results`)
- `--output_dir`: Output directory (default: `plots`)

## Input Files
CSV files with naming pattern:
```
<controller>-<workload_type>-encryption_<encryption>-logging_<logging>-connection_<connection>.csv
```

**Examples:**
- `direct_CVM-query_mgmt_medium-encryption_OFF-logging_OFF-connection_UNIX.csv`
- `gdpr_bare_metal-query_mgmt_medium-encryption_ON-logging_OFF-connection_TCP.csv`
- `gdpr_CVM-query_mgmt_medium-encryption_ON-logging_ON-connection_UNIX.csv`

**Required columns:**
- `workload` - YCSB workload name
- `controller` - Database controller type
- `db` - Database system (redis/rocksdb)
- `n_clients` - Number of threads
- `elapsed_time (s)` - Execution time
- `avg_latency (s)` - Average latency

**Additional columns (for logging analysis):**
- `ctl_files_count` - Number of controller files created
- `ctl_files_size_mb` - Size of controller files (MB)
- `db_files_count` - Number of database files created  
- `db_files_size_mb` - Size of database files (MB)

*Note: Extra columns are ignored by the standard performance scripts*

## Data Processing

The scripts automatically:
- **Filter data**: Performance scripts use `logging=OFF`, logging script uses `logging=ON`
- **Ignore extra columns** (last 4 columns referring to storage are skipped explicitly)
- **Calculate throughput** from elapsed time
- **Convert units** (latency to μs, throughput to kops)
- **Generate error bars** from standard deviation

## System Components

### Controller Types
- **direct**: Direct database access
- **passthrough**: Proxy access
- **gdpr**: GDPRuler access

### YCSB Workloads

- **A**: 50% reads, 50% updates (write-heavy)
- **B**: 95% reads, 5% updates 
- **C**: 100% reads (read-only)
- **D**: 95% reads, 5% inserts
- **F**: 50% reads, 50% read-modify-writes

### Environment Types
- **bare_metal**: Native hardware execution
- **CVM**: Confidential Virtual Machine execution

## Output

### `generate_plots.py`
```
plots/
├── redis/
│ ├── redis_latency_workloada.png/pdf
│ ├── redis_throughput_workloada.png/pdf
│ └── ...
└── rocksdb/
├── rocksdb_latency_workloada.png/pdf
└── ...
```

### `generate_ycsb_performance_plot.py`
```
plots/
├── ycsb_performance_plot.png/pdf
└── ycsb_performance_plot_tcp.png/pdf
```

### `generate_logging_plot_and_stats.py`
```
plots/
├── logging_impact_gdpr_CVM_8_clients_encryption_ON.png/pdf
└── logging_impact_gdpr_bare_metal_8_clients_encryption_ON.png/pdf
```

**Plus, detailed console output, such as:**
```
================================================================================
STORAGE STATISTICS
================================================================================
REDIS DATABASE:
----------------------------------------
WORKLOADA:
  Baseline (0% logged):
    Controller files: 0 files, 0.00 MB
    DB files: 1 files, 112.33 MB
    Throughput: 75.36 kops/s
  10% logged:
    Controller files: 922 files (+∞%), 6.52 MB (+∞%)
    DB files: 1 files (+0.0%), 112.33 MB (+0.0%)
    Throughput: 73.08 kops/s (-3.02%)
```


## Key Features

- **Comprehensive analysis**: Multiple visualization approaches for different use cases
- **Error visualization**: Standard deviation bars show measurement reliability  
- **Configurable filtering**: Focus on specific variants, thread counts, or encryption settings
- **Professional output**: Publication-ready plots with proper formatting and legends
- **Storage impact analysis**: Quantify logging overhead in terms of files and performance
- **Backward compatibility**: Works with both old (6-column) and new (10-column) data formats
- **Multiple formats**: PNG for presentations, PDF for publications
