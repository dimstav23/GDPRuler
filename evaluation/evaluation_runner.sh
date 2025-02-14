#!/bin/sh

script_dir=$(dirname "$(readlink -f "$0")")

cd $script_dir/bare_metal
./automated_runner.sh

cd $script_dir/VM
./automated_runner.sh
