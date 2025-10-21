#!/bin/bash

set -x

VM=(amd snp)
DISKS=${DISKS:-nvme1n1}

# Options for SWIOTLB
# SWIOTLB_OPTIONS=(
#   '--virtio-iommu --extra-cmdline "swiotlb=524288,force"'
#   '--extra-cmdline "idle=poll" --name-extra -poll'
#   '--extra-cmdline "cpuidle_haltpoll.force=Y" --name-extra -haltpoll'
# )
SWIOTLB_OPTIONS=(\
  '--extra-cmdline "idle=poll" --name-extra -poll'
)

for size in medium
do
    for type_ in "${VM[@]}"
    do
        for action in fio
        do
            for disk in $DISKS
            do
                for SWIOTLB_OPTION in "${SWIOTLB_OPTIONS[@]}"
                do
                    inv vm.start --type ${type_} --size ${size} --virtio-blk /dev/$disk --no-warn --action="run-${action}"  --fio-job "libaio" $SWIOTLB_OPTION
                done
            done
        done
    done
done

for size in medium
do
    for type_ in "${VM[@]}"
    do
        # for action in ping iperf iperf-udp
        for action in iperf
        do
            for SWIOTLB_OPTION in "${SWIOTLB_OPTIONS[@]}"
            do
              inv vm.start --type ${type_} --size ${size} --virtio-nic --action="run-${action}" --virtio-nic-tap="tap_cvm" $SWIOTLB_OPTION
              inv vm.start --type ${type_} --size ${size} --virtio-nic --action="run-${action}" --virtio-nic-tap="tap_cvm"  --virtio-nic-vhost $SWIOTLB_OPTION
              inv vm.start --type ${type_} --size ${size} --virtio-nic --action="run-${action}" --virtio-nic-mtap="mtap_cvm" --virtio-nic-vhost --virtio-nic-mq $SWIOTLB_OPTION
              inv vm.start --type ${type_} --size ${size} --virtio-nic --action="run-${action}" --virtio-nic-mtap="mtap_cvm" --virtio-nic-mq $SWIOTLB_OPTION
            done
        done
    done
done

for size in medium
do
    for type_ in "${VM[@]}"
    do
        for action in memtier
        do
            for SWIOTLB_OPTION in "${SWIOTLB_OPTIONS[@]}"
            do
              inv vm.start --type ${type_} --size ${size} --virtio-nic --action="run-${action}" --virtio-nic-tap="tap_cvm" $SWIOTLB_OPTION
              inv vm.start --type ${type_} --size ${size} --virtio-nic --action="run-${action}" --virtio-nic-tap="tap_cvm" --tls $SWIOTLB_OPTION
              inv vm.start --type ${type_} --size ${size} --virtio-nic --action="run-${action}" --virtio-nic-tap="tap_cvm" --virtio-nic-vhost $SWIOTLB_OPTION
              inv vm.start --type ${type_} --size ${size} --virtio-nic --action="run-${action}" --virtio-nic-tap="tap_cvm" --tls --virtio-nic-vhost $SWIOTLB_OPTION 
              inv vm.start --type ${type_} --size ${size} --virtio-nic --action="run-${action}" --virtio-nic-mtap="mtap_cvm" --virtio-nic-mq $SWIOTLB_OPTION
              inv vm.start --type ${type_} --size ${size} --virtio-nic --action="run-${action}" --virtio-nic-mtap="mtap_cvm" --tls --virtio-nic-mq $SWIOTLB_OPTION
              inv vm.start --type ${type_} --size ${size} --virtio-nic --action="run-${action}" --virtio-nic-mtap="mtap_cvm" --virtio-nic-vhost --virtio-nic-mq $SWIOTLB_OPTION
              inv vm.start --type ${type_} --size ${size} --virtio-nic --action="run-${action}" --virtio-nic-mtap="mtap_cvm" --tls --virtio-nic-vhost --virtio-nic-mq $SWIOTLB_OPTION
            done
        done
    done
done

