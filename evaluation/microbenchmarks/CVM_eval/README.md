# CVM-eval
This repository contains configurations to create a development environment as well as evaluation scripts for AMD SEV-SNP and Intel TDX.

## Support status
- [x] AMD SEV-SNP
    - The current main is tested on Linux 6.9
- [x] Intel TDX
    - The current main is tested on Linux 6.8 (canonical/tdx)

See [./docs/software_version.md](./docs/software_version.md) for the detail of the software version.

## Development
See [./docs/development.md](./docs/development.md)

## License
- MIT (unless explicitly noted, e.g., some kernel modules have different license described in the file)

## Updated tracer notes
For the GDPR analysis project:
Run the [`tracer`](./experiment/tracer.sh):
```
cd experiment
bash tracer.sh
```
This script applies the [`tracer patch`](./perf_trace.patch) (if not already applied) and runs the respective experiments specified in the script.
It stores its results in the [`trace-result`] directory.

To provide meaningful results out of these overly complicated results, you can use the [`plot_trace_metrics.sh`](./plot_trace_metrics.sh) script that analyses the results (CPU, memory, I/O, interrupts) and produces both `png`s and interactive `html` plots that are placed in the (created) [`trace/plotting_scripts/plots`] directory.