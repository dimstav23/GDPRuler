#!/bin/bash

set -x

# auto detect if possible
lscpu | grep -q AMD-V > /dev/null
if [ $? -eq 0 ]; then
    CVM=${CVM:-"snp"}
else
    CVM=${CVM:-"tdx"}
fi

RESULTDIR=${RESULTDIR:-"bench-result"}
OUT=${OUT:-"plot"}

set -e
set -u
set -o pipefail

inv storage.plot-fio --cvm $CVM --device nvme1n1 --outdir $OUT --result-dir $RESULTDIR/fio
inv network.plot-network --cvm $CVM --mode tcp --outdir $OUT --result-dir $RESULTDIR/network
inv network.plot-network --cvm $CVM --mode tcp --mq --outdir $OUT --result-dir $RESULTDIR/network
# inv network.plot-memcached --cvm $CVM --outdir $OUT --result-dir $RESULTDIR/network

# inv network.plot-iperf --cvm $CVM --mode tcp --outdir $OUT --result-dir $RESULTDIR/network
# inv network.plot-iperf --cvm $CVM --mode tcp --mq --outdir $OUT --result-dir $RESULTDIR/network
# inv network.plot-redis --cvm $CVM --outdir $OUT --result-dir $RESULTDIR/network
# inv network.plot-redis --cvm $CVM --outdir $OUT --mq --result-dir $RESULTDIR/network

# inv network.plot-iperf --cvm $CVM --mode tcp --mq --pkt 128K --outdir $OUT --result-dir $RESULTDIR/network
# inv network.plot-iperf --cvm $CVM --mode tcp --pkt 128K --plot-all --outdir $OUT --result-dir $RESULTDIR/network

# inv network.plot-ping --cvm $CVM --outdir $OUT --result-dir $RESULTDIR/network
# inv network.plot-ping --cvm $CVM --mq --outdir $OUT --result-dir $RESULTDIR/network
# inv network.plot-iperf --cvm $CVM --mode udp --outdir $OUT --result-dir $RESULTDIR/network
# inv network.plot-iperf --cvm $CVM --mode udp --mq --outdir $OUT --result-dir $RESULTDIR/network
# inv network.plot-iperf --cvm $CVM --mode tcp --outdir $OUT --result-dir $RESULTDIR/network
# inv network.plot-iperf --cvm $CVM --mode tcp --mq --outdir $OUT --result-dir $RESULTDIR/network
# inv network.plot-iperf --cvm $CVM --mode udp --plot-all --outdir $OUT --result-dir $RESULTDIR/network
# inv network.plot-iperf --cvm $CVM --mode udp --mq --plot-all --outdir $OUT --result-dir $RESULTDIR/network
# inv network.plot-iperf --cvm $CVM --mode tcp --mq --pkt 128K --plot-all --outdir $OUT --result-dir $RESULTDIR/network
# inv network.plot-redis --cvm $CVM --outdir $OUT --result-dir $RESULTDIR/network
# inv network.plot-redis --cvm $CVM --mq --outdir $OUT --result-dir $RESULTDIR/network
