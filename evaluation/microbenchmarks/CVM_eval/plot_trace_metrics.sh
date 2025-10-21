#!/bin/bash
set -euxo pipefail

PLOT_DIR="trace/plotting_scripts"

cd $PLOT_DIR
python3 plot_cpu.py --redis --fio --network --memcached
python3 plot_memory.py --redis --fio --network --memcached
python3 plot_network_io.py --redis --fio --network --memcached
python3 plot_storage_io.py --redis --fio --network --memcached
python3 plot_interrupts.py --redis --fio --network --memcached
