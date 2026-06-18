# GDPRuler AE

This AE folder is intentionally minimal and only contains:

- `README.md`
- `run_ae.sh`

The script follows your tested semi-automated flow and assumes the repository is cloned as `GDPRuler_AE`.

## Usage

From repository root:

```bash
cd AE
chmod +x run_ae.sh
./run_ae.sh
```

Custom scratch base (instead of `/scratch/dimitrios`):

```bash
./run_ae.sh --scratch-base /scratch/<your-user>
```

The script reads plot input results from:

- `<scratch-base>/<repo-name>/evaluation/VM/results`
- `<scratch-base>/<repo-name>/evaluation/bare_metal/results`

## What `run_ae.sh` does

1. Generates YCSB workloads
2. Builds the logging subsystem (`BUILD_BENCHMARKS=ON`)
3. Builds controller
4. Builds Redis
5. Builds CVM image/setup (`ovmf` + `GDPRuler_VMs_setup.sh`)
6. Runs core evaluation (`evaluation/evaluation_runner.sh`)
7. Runs CVM-eval microbenchmarks (`evaluation/microbenchmarks/run_CVM_eval_benchmarks.sh`)
8. Runs logger benchmarks + logger plot
9. Runs final evaluation plots

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

