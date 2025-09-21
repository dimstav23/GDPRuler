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

## Requirements
The required packages are already include in the nix development environment.
```
pandas matplotlib seaborn
```

## Usage
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

## Input Files
CSV files with naming pattern:
```
<controller>-<workload_type>-encryption_<encryption>-logging_<logging>-connection_<connection>.csv
```

**Examples:**
- `direct_CVM-query_mgmt_medium-encryption_OFF-logging_OFF-connection_UNIX.csv`
- `gdpr_bare_metal-query_mgmt_medium-encryption_ON-logging_OFF-connection_TCP.csv`

**Required columns:**
- `workload` - YCSB workload name
- `controller` - Database controller type
- `db` - Database system (redis/rocksdb)
- `n_clients` - Number of threads
- `elapsed_time (s)` - Execution time
- `avg_latency (s)` - Average latency

If there are more columns (e.g., storage information), they are ignored.

## Data Processing

The scripts automatically:
- **Filter logging=OFF only** (explicitly ignores `logging=ON` cases)
- **Ignore extra columns** (last 4 columns referring to storage are skipped explicitly)
- **Calculate throughput** from elapsed time
- **Convert units** (latency to μs, throughput to kops)
- **Generate error bars** from standard deviation

## Controller Types
- **direct**: Direct database access
- **passthrough**: Proxy access
- **gdpr**: GDPRuler access

## YCSB Workloads

- **A**: 50% reads, 50% updates (write-heavy)
- **B**: 95% reads, 5% updates 
- **C**: 100% reads (read-only)
- **D**: 95% reads, 5% inserts
- **F**: 50% reads, 50% read-modify-writes

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
