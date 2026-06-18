#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
REPO_NAME="$(basename "$REPO_ROOT")"

SCRATCH_BASE="/scratch/dimitrios"
NO_NIX="false"
INSIDE_NIX="false"

usage() {
  cat <<EOF
Usage: $(basename "$0") [--scratch-base <absolute_path>] [--no-nix] [-h|--help]

Options:
  --scratch-base <path>  Base scratch directory (default: /scratch/dimitrios)
                         Results are read from:
                         <scratch-base>/<repo-name>/evaluation/{VM,bare_metal}/results
  --no-nix               Skip automatic 'nix develop' re-exec
  -h, --help             Show this help
EOF
}

parse_args() {
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --scratch-base)
        SCRATCH_BASE="${2:-}"
        shift 2
        ;;
      --no-nix)
        NO_NIX="true"
        shift
        ;;
      --inside-nix)
        INSIDE_NIX="true"
        shift
        ;;
      -h|--help)
        usage
        exit 0
        ;;
      *)
        echo "[AE][ERROR] Unknown argument: $1" >&2
        usage
        exit 1
        ;;
    esac
  done

  if [[ -z "$SCRATCH_BASE" || "$SCRATCH_BASE" != /* ]]; then
    echo "[AE][ERROR] --scratch-base must be an absolute path" >&2
    exit 1
  fi
}

ensure_nix_shell() {
  local script_path="${SCRIPT_DIR}/run_ae.sh"

  if [[ "$NO_NIX" == "true" ]]; then
    log "Skipping nix shell entry (--no-nix provided)"
    return
  fi

  if [[ -n "${IN_NIX_SHELL:-}" || "$INSIDE_NIX" == "true" ]]; then
    log "Already running inside nix shell"
    return
  fi

  require_cmd nix
  log "Entering nix development shell via 'nix develop'"
  exec nix develop -c bash "$script_path" --inside-nix "$@"
}

log() {
  echo "[AE] $*"
}

require_cmd() {
  command -v "$1" >/dev/null 2>&1 || {
    echo "[AE][ERROR] Missing command: $1" >&2
    exit 1
  }
}

init_submodules() {
  log "Resetting submodule directories and re-initializing from scratch"

  while IFS= read -r path; do
    local abs_path="${REPO_ROOT}/${path}"
    if [[ -e "$abs_path" ]]; then
      log "Removing existing submodule path: ${abs_path}"
      rm -rf "$abs_path"
    fi
  done < <(git config --file .gitmodules --get-regexp '^submodule\..*\.path$' | awk '{print $2}')

  git submodule deinit -f --all || true
  git submodule sync --recursive
  git submodule update --init --recursive --force
}

check_repo_name() {
  local base
  base="$(basename "$REPO_ROOT")"
  if [[ "$base" != "GDPRuler_AE_bare" ]]; then
    log "Warning: repository directory is '$base' (expected 'GDPRuler_AE_bare'). Continuing..."
  fi
}

main() {
  local original_args=("$@")
  parse_args "$@"
  ensure_nix_shell "${original_args[@]}"

  require_cmd bash
  require_cmd git
  require_cmd cmake
  require_cmd make
  require_cmd python3

  local scratch_repo_root="${SCRATCH_BASE}/${REPO_NAME}"
  local vm_results_dir="${scratch_repo_root}/evaluation/VM/results"
  local bare_metal_results_dir="${scratch_repo_root}/evaluation/bare_metal/results"
  local final_plots_dir="${REPO_ROOT}/evaluation/plots/final_plots"

  check_repo_name
  cd "$REPO_ROOT"

  log "Repository root: ${REPO_ROOT}"
  log "Scratch base: ${SCRATCH_BASE}"
  log "Plot input (VM): ${vm_results_dir}"
  log "Plot input (bare-metal): ${bare_metal_results_dir}"

  log "Step 0/10: Initialize and update git submodules"
  (
    cd "${REPO_ROOT}"
    init_submodules
  )

  log "Step 1/10: Generate YCSB workloads"
  (
    cd "${REPO_ROOT}/ycsb_trace_generator"
    bash workload_generator.sh
  )

  log "Step 2/10: Build logging subsystem (benchmarks ON)"
  (
    cd "${REPO_ROOT}/gdpr-logger"
    mkdir -p build
    cd build
    cmake .. -D CMAKE_BUILD_TYPE=Release -D BUILD_BENCHMARKS=ON
    make -j"$(nproc)"
  )

  log "Step 3/10: Build GDPR controller"
  (
    cd "${REPO_ROOT}/controller"
    cmake -S . -B build -D CMAKE_BUILD_TYPE=Release
    cmake --build build
  )

  log "Step 4/10: Build Redis"
  (
    cd "${REPO_ROOT}/KVs/redis"
    make BUILD_TLS=yes MALLOC=libc
  )

  log "Step 5/10: Build CVM image/setup"
  (
    cd "${REPO_ROOT}/CVM_setup/AMDSEV"
    bash ./build.sh ovmf
    cd ../
    bash GDPRuler_VMs_setup.sh
  )

  log "Step 6/10: Run core bare-metal + CVM evaluation"
  log "Note: this may fail in CVM phase if CVM lacks internet access to fetch croaring"
  (
    cd "${REPO_ROOT}/evaluation"
    bash evaluation_runner.sh
  )

  log "Step 7/10: Run CVM-eval microbenchmarks"
  (
    cd "${REPO_ROOT}/evaluation/microbenchmarks"
    bash run_CVM_eval_benchmarks.sh
  )

  log "Step 8/10: Run logger benchmarks and logger plot"
  (
    cd "${REPO_ROOT}/gdpr-logger"
    mkdir -p build
    cd build
    cmake .. -D CMAKE_BUILD_TYPE=Release -D BUILD_EXAMPLES=ON -D BUILD_TESTING=ON -D BUILD_BENCHMARKS=ON
    make -j"$(nproc)"
    ./gdpruler_log_performance_benchmark
    ./gdpruler_compression_rate_benchmark
    cd ../plot_scripts
    python3 gdpruler_benchmark_plot.py --input_file ../build/gdpr_logger_benchmark_results.csv
  )

  log "Step 9/10: Generate final evaluation plots"
  (
    cd "${REPO_ROOT}/evaluation/plots"
    mkdir -p "$final_plots_dir"
    python3 generate_ycsb_performance_plot.py \
      --output_dir "$final_plots_dir" \
      --vm_results "$vm_results_dir" \
      --bare_metal_results "$bare_metal_results_dir" \
      > output_ycsb.txt

    python3 generate_gdpr_queries_plots.py \
      --output_dir "$final_plots_dir" \
      --vm_results "$vm_results_dir" \
      --bare_metal_results "$bare_metal_results_dir" \
      > output_gdpr_queries.txt

    python3 generate_logging_plot_and_stats.py \
      --output_dir "$final_plots_dir" \
      --vm_results "$vm_results_dir" \
      --bare_metal_results "$bare_metal_results_dir"
  )

  log "AE run flow completed"
}

main "$@"
