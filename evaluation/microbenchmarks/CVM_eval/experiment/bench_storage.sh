#!/bin/bash

set -x

VM=(amd snp)
DISKS=${DISKS:-nvme1n1}

for size in medium
do
    for type_ in "${VM[@]}"
    do
        for action in fio
        do
            for disk in ${DISKS}
            do
                inv vm.start --type ${type_} --size ${size} --virtio-blk /dev/${disk} --no-warn --action="run-${action}"  --fio-job "libaio"
            done
        done
    done
done


