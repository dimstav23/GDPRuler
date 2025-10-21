#!/bin/bash

set -x
set -e
set -u
set -o pipefail

DISKS=${DISKS:-nvme1n1}

# Network
for size in medium
do
    # for action in ping iperf iperf-udp
    for action in iperf
    do
        inv bare-metal.start --size ${size} --action="run-${action}"
    done
done

for size in medium
do
    for action in memtier
    do
        inv bare-metal.start --size ${size} --action="run-${action}"
        inv bare-metal.start --size ${size} --action="run-${action}" --tls
    done
done

# Storage
for size in medium
do
    for action in fio
    do
        for disk in ${DISKS}
        do
            inv bare-metal.start --size ${size} --fio-filename /dev/${disk} --action="run-${action}"  --fio-job "libaio"
        done
    done
done