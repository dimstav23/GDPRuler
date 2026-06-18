# GDPRuler AE

This AE folder is intentionally minimal and only contains:

- `README.md`
- `run_ae.sh`

The script follows your tested semi-automated flow and assumes the repository is cloned as `GDPRuler`.

## Repository roadmap

Use the top-level project documentation for directory-level orientation and baseline build instructions:

- [Root README](../README.md): overall repository structure and core build steps.
- [Evaluation README](../evaluation/README.md): bare-metal and CVM evaluation script behavior and output conventions.
- [Plot README](../evaluation/plots/README.md): plotting scripts, inputs, and expected plot outputs.

Important directories used during AE:

- `controller/`: GDPR controller implementation and build outputs.
- `gdpr-logger/`: logging subsystem and logger benchmarks.
- `evaluation/`: core automated evaluation (bare-metal + CVM) and result CSVs.
- `evaluation/microbenchmarks/`: CVM-eval microbenchmark automation.
- `evaluation/plots/`: scripts that regenerate paper-oriented plots.
- `ycsb_trace_generator/`: generation of YCSB/GDPR workload traces.
- `workload_traces/`: generated traces consumed by clients.

## Usage

From repository root:

```bash
cd AE
chmod +x run_ae.sh
./run_ae.sh
```

By default, `run_ae.sh` automatically enters the nix dev shell (`nix develop`) if you are not already inside one.

If you are already in a prepared environment and want to skip that behavior:

```bash
./run_ae.sh --no-nix
```

Custom scratch base (instead of `/scratch/dimitrios`):

```bash
./run_ae.sh --scratch-base /scratch/<your-user>
```

The script reads plot input results from:

- `<scratch-base>/GDPRuler/evaluation/VM/results`
- `<scratch-base>/GDPRuler/evaluation/bare_metal/results`

## What `run_ae.sh` does

1. Initializes/updates git submodules (`git submodule update --init --recursive`)
2. Generates YCSB workloads
3. Builds the logging subsystem (`BUILD_BENCHMARKS=ON`)
4. Builds controller
5. Builds Redis
6. Builds CVM image/setup (`ovmf` + `GDPRuler_VMs_setup.sh`)
7. Runs core evaluation (`evaluation/evaluation_runner.sh`)
8. Runs CVM-eval microbenchmarks (`evaluation/microbenchmarks/run_CVM_eval_benchmarks.sh`)
9. Runs logger benchmarks + logger plot
10. Runs final evaluation plots

`run_ae.sh` performs a destructive submodule reset: if submodule directories already exist, they are removed and submodules are re-initialized from scratch.

## Installation + execution quick tutorial (minimum)

From repository root:

```bash
cd AE
chmod +x run_ae.sh
./run_ae.sh --scratch-base /scratch/<your-user>
```

By default the script re-executes itself inside `nix develop` (unless `--no-nix` is provided).

## Expected outputs and claim mapping

The artifact is intended to support the following major claims from the paper at a practical AE level:

- **C1 (end-to-end performance):** GDPRuler performance under bare-metal and CVM can be reproduced with YCSB workloads.
  - Main outputs: CSVs in `evaluation/bare_metal/results` and `evaluation/VM/results`.
  - Main plots: files in `evaluation/plots/final_plots` produced by `generate_ycsb_performance_plot.py`.

- **C2 (GDPR query acceleration):** metadata-index-aware GDPR query experiments can be reproduced and plotted.
  - Main plots: files in `evaluation/plots/final_plots` produced by `generate_gdpr_queries_plots.py`.

- **C3 (logging overhead and storage impact):** logging benchmarks and logging impact plots can be reproduced.
  - Logger benchmark CSV: `gdpr-logger/build/gdpr_logger_benchmark_results.csv`.
  - Logger plots: `gdpr-logger/plot_scripts/` output and `evaluation/plots/final_plots` output from `generate_logging_plot_and_stats.py`.

The AEC should expect independent reruns to produce trends consistent with the paper rather than bit-identical numbers.

## Output notes

- Core evaluation results are produced by existing scripts in:
  - `evaluation/bare_metal/results`
  - `evaluation/VM/results`
- Final plots are written to:
  - `evaluation/plots/final_plots` (absolute path under your repo root)
- Plot command logs are written to:
  - `evaluation/plots/output_ycsb.txt`
  - `evaluation/plots/output_gdpr_queries.txt`

## Known issue reminder

During `evaluation_runner.sh`, CVM runs may fail if the CVM cannot fetch dependencies (e.g., `croaring`) from GitHub due to network access.

## Known limitations / expectation setting

- Full CVM execution requires a correctly configured AMD-SEV-SNP environment and VM network access.
- End-to-end runs can take multiple hours depending on machine and storage.
- Some scripts assume scratch-backed storage layout under the configured scratch base.
- The AE script is intentionally automation-first and destructive for submodule paths (fresh re-init every run).

These points are intentional to avoid reverse-engineering burden and to set clear evaluator expectations up front.

## Networking and storage configuration notes

The end-to-end AE run uses multiple scripts with explicit network/storage defaults. If your testbed differs, adjust the following locations.

### 1) Virtual bridge and CVM network

- Bridge name is hardcoded as `virbr0` in:
  - `evaluation/common.sh` (CVM launch uses `-bridge virbr0`)
  - `CVM_setup/GDPRuler_VMs_setup.sh` (`BRIDGE_NAME=virbr0`)
- Bridge IP default in VM setup script: `192.168.122.1` (`BRIDGE_IP` in `CVM_setup/GDPRuler_VMs_setup.sh`).
- CVM guest static IP defaults to `.48`:
  - `evaluation/common.sh`: `CONFIG[CVM_IP]=192.168.122.48`
  - `evaluation/VM/config.sh`: `direct_*_address` and `controller_address` are `192.168.122.48`
  - `CVM_setup/network_configs/netplan-gdpr.yaml`: guest address/gateway template.

If you change bridge subnet or guest IP, update all of the above consistently.

`CVM_setup/prepare_net_cfg.sh` can rewrite netplan prefix to match the bridge subnet, but the host-side CVM IP constants in `evaluation/common.sh` and `evaluation/VM/config.sh` still need to match the final guest IP.

### 2) CVM-eval microbenchmark networking

- `evaluation/microbenchmarks/run_CVM_eval_benchmarks.sh` also relies on bridge/tap setup and uses:
  - `CVM_setup/network_configs/netplan-gdpr-cvm-eval.yaml`
  - `nix develop -c just setup_bridge`
  - `nix develop -c just setup_tap`

If CVM-eval networking fails, verify these bridge/tap steps and the netplan file used by this script.

### 3) Scratch / DB / log storage paths

- In `AE/run_ae.sh`, `--scratch-base` controls where plotting scripts read result CSVs from:
  - `<scratch-base>/<repo-name>/evaluation/VM/results`
  - `<scratch-base>/<repo-name>/evaluation/bare_metal/results`

- Runtime DB/log storage for evaluation scripts is configured in `evaluation/common.sh`:
  - `NVME_DEVICE` (default `/dev/nvme1n1`, override via `GDPRULER_NVME_DEVICE`)
  - `MOUNT_POINT` (default `/scratch/dimitrios/gdpruler_fs`, override via `GDPRULER_MOUNT_POINT`)
  - `CONFIG[DB_DUMP_DIR]` (default `/scratch/$USER/gdpruler_fs/db_data`)
  - `CONFIG[CTL_DUMP_DIR]` (default `/scratch/$USER/gdpruler_fs/controller_data`)

Important: `prepare_storage_host()` may create a filesystem (`mkfs`) on `NVME_DEVICE` and mount it at `MOUNT_POINT`; configure this carefully for your server.

### Environment variable reference

```bash
# Mount point for evaluation storage (used by evaluation/common.sh)
export GDPRULER_MOUNT_POINT=/path/to/your/mount/point

# NVMe device for filesystem operations (used by evaluation/common.sh)
export GDPRULER_NVME_DEVICE=/dev/your/device
```

All environment variables have sensible defaults pointing to `/scratch/dimitrios/` and are optional.

### 4) What to adjust first on a new server

1. `evaluation/common.sh`: set `NVME_DEVICE`, `MOUNT_POINT`, `CONFIG[CVM_IP]`, and (if needed) bridge name in `boot_cvm()`.
2. `evaluation/VM/config.sh`: set `direct_*_address` and `controller_address` to the same CVM IP.
3. `CVM_setup/GDPRuler_VMs_setup.sh`: set `BRIDGE_NAME` / `BRIDGE_IP`.
4. `CVM_setup/network_configs/*.yaml`: align guest IP/gateway with your bridge subnet.
