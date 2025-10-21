#!/bin/bash

set -e
set -u
set -o pipefail

# Enable debug mode if DEBUG is set
if [[ "${DEBUG:-}" == "1" ]]; then
    set -x
fi

# Function to print colored output
print_status() {
    echo -e "\033[1;32m[INFO]\033[0m $1"
}

print_warning() {
    echo -e "\033[1;33m[WARNING]\033[0m $1"
}

print_error() {
    echo -e "\033[1;31m[ERROR]\033[0m $1"
}

# Get script directory (works even when sourced or called from anywhere)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="${SCRIPT_DIR}/../"
TRACE_PATCH="${SCRIPT_DIR}/../perf_trace.patch"

print_status "Script directory: $SCRIPT_DIR"
print_status "Root directory: $ROOT_DIR"
print_status "Patch file: $TRACE_PATCH"

# Validate directories and files exist
if [[ ! -d "$ROOT_DIR" ]]; then
    print_error "Root directory does not exist: $ROOT_DIR"
    exit 1
fi

if [[ ! -f "$TRACE_PATCH" ]]; then
    print_error "Patch file does not exist: $TRACE_PATCH"
    exit 1
fi

# Change to root directory
cd "$ROOT_DIR" || {
    print_error "Failed to change to root directory: $ROOT_DIR"
    exit 1
}

# Check if we're in a git repository
if ! git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    print_error "Not in a git repository"
    exit 1
fi

# Check for uncommitted changes
if ! git diff-index --quiet HEAD --; then
    print_warning "You have uncommitted changes in the repository"
    read -p "Do you want to continue? (y/N): " -r
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        print_status "Aborted by user"
        exit 0
    fi
fi

# Check if patch can be applied (dry run)
print_status "Testing if patch can be applied..."
if ! git apply --check "$TRACE_PATCH" 2>/dev/null; then
    print_error "Patch cannot be applied cleanly"
    
    # Check if patch is already applied
    if git apply --reverse --check "$TRACE_PATCH" 2>/dev/null; then
        print_warning "Patch appears to already be applied"
        read -p "Do you want to reverse the patch first? (y/N): " -r
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            print_status "Reversing patch..."
            if git apply --reverse "$TRACE_PATCH"; then
                print_status "Patch reversed successfully"
                print_status "Now applying patch..."
                if git apply "$TRACE_PATCH"; then
                    print_status "Patch applied successfully"
                else
                    print_error "Failed to apply patch after reversing"
                    exit 1
                fi
            else
                print_error "Failed to reverse patch"
                exit 1
            fi
        else
            print_status "Aborted by user"
            exit 0
        fi
    else
        print_error "Patch conflicts with current state. Please resolve conflicts manually."
        exit 1
    fi
else
    # Apply patch
    print_status "Applying patch..."
    if git apply "$TRACE_PATCH"; then
        print_status "Patch applied successfully"
    else
        print_error "Failed to apply patch"
        exit 1
    fi
fi

# Return to script directory
cd "$SCRIPT_DIR" || {
    print_error "Failed to return to script directory: $SCRIPT_DIR"
    exit 1
}

print_status "Patch application completed successfully"
print_status "Current directory: $(pwd)"

VM=(amd snp)
DISKS=${DISKS:-nvme1n1}

# Options for SWIOTLB
SWIOTLB_OPTIONS=(
  '--virtio-iommu --extra-cmdline "swiotlb=524288,force"'
  '--extra-cmdline "idle=poll" --name-extra -poll'
  '--extra-cmdline "cpuidle_haltpoll.force=Y" --name-extra -haltpoll'
)

SWIOTLB_OPTIONS_TAGS=(
  'swiotlb'
  'poll'
  'hpoll'
)

for size in medium
do
    for type_ in "${VM[@]}"
    do
        for action in iperf
        do
            just trace ${type_}_network 30 &
            inv vm.start --type ${type_} --size ${size} --virtio-nic --action="run-${action}" --virtio-nic-tap="tap_cvm"
            sleep 5
            just trace ${type_}_vhost_network 30 &
            inv vm.start --type ${type_} --size ${size} --virtio-nic --action="run-${action}" --virtio-nic-tap="tap_cvm"  --virtio-nic-vhost
            sleep 5
            just trace ${type_}_vhost_mq_network 30 &
            inv vm.start --type ${type_} --size ${size} --virtio-nic --action="run-${action}" --virtio-nic-mtap="mtap_cvm" --virtio-nic-vhost --virtio-nic-mq
            sleep 5
            just trace ${type_}_mq_network 30 &
            inv vm.start --type ${type_} --size ${size} --virtio-nic --action="run-${action}" --virtio-nic-mtap="mtap_cvm" --virtio-nic-mq
            sleep 5
        done
    done
done

for size in medium
do
    for type_ in "${VM[@]}"
    do
        for action in memtier
        do
            just trace ${type_}_memtier_redis 30 &
            inv vm.start --type ${type_} --size ${size} --virtio-nic --action="run-${action}" --virtio-nic-tap="tap_cvm"
            sleep 5
            just trace ${type_}_vhost_memtier_redis 30 &
            inv vm.start --type ${type_} --size ${size} --virtio-nic --action="run-${action}" --virtio-nic-tap="tap_cvm" --virtio-nic-vhost
            sleep 5
            just trace ${type_}_mq_memtier_redis 30 &
            inv vm.start --type ${type_} --size ${size} --virtio-nic --action="run-${action}" --virtio-nic-mtap="mtap_cvm" --virtio-nic-mq
            sleep 5
            just trace ${type_}_vhost_mq_memtier_redis 30 &
            inv vm.start --type ${type_} --size ${size} --virtio-nic --action="run-${action}" --virtio-nic-mtap="mtap_cvm" --virtio-nic-vhost --virtio-nic-mq
            sleep 5
        done
    done
done

for size in medium
do
    for type_ in "${VM[@]}"
    do
        for action in memtier-memcached
        do
            just trace ${type_}_memcached 30 &
            inv vm.start --type ${type_} --size ${size} --virtio-nic --action="run-${action}" --virtio-nic-tap="tap_cvm"
            sleep 5
            just trace ${type_}_vhost_memcached 30 &
            inv vm.start --type ${type_} --size ${size} --virtio-nic --action="run-${action}" --virtio-nic-tap="tap_cvm" --virtio-nic-vhost
            sleep 5
            just trace ${type_}_mq_memcached 30 &
            inv vm.start --type ${type_} --size ${size} --virtio-nic --action="run-${action}" --virtio-nic-mtap="mtap_cvm" --virtio-nic-mq
            sleep 5
            just trace ${type_}_vhost_mq_memcached 30 &
            inv vm.start --type ${type_} --size ${size} --virtio-nic --action="run-${action}" --virtio-nic-mtap="mtap_cvm" --virtio-nic-vhost --virtio-nic-mq
            sleep 5
        done
    done
done

for size in medium
do
    for type_ in "${VM[@]}"
    do
        for action in iperf
        do
            for i in "${!SWIOTLB_OPTIONS[@]}"
            do
                SWIOTLB_OPTION="${SWIOTLB_OPTIONS[$i]}"
                SWIOTLB_OPTION_TAG="${SWIOTLB_OPTIONS_TAGS[$i]}"
                just trace ${type_}_${SWIOTLB_OPTION_TAG}_network 30 &
                inv vm.start --type ${type_} --size ${size} --virtio-nic --action="run-${action}" --virtio-nic-tap="tap_cvm" $SWIOTLB_OPTION
                sleep 5
                just trace ${type_}_${SWIOTLB_OPTION_TAG}_vhost_network 30 &
                inv vm.start --type ${type_} --size ${size} --virtio-nic --action="run-${action}" --virtio-nic-tap="tap_cvm"  --virtio-nic-vhost $SWIOTLB_OPTION
                sleep 5
                just trace ${type_}_${SWIOTLB_OPTION_TAG}_vhost_mq_network 30 &
                inv vm.start --type ${type_} --size ${size} --virtio-nic --action="run-${action}" --virtio-nic-mtap="mtap_cvm" --virtio-nic-vhost --virtio-nic-mq $SWIOTLB_OPTION
                sleep 5
                just trace ${type_}_${SWIOTLB_OPTION_TAG}_mq_network 30 &
                inv vm.start --type ${type_} --size ${size} --virtio-nic --action="run-${action}" --virtio-nic-mtap="mtap_cvm" --virtio-nic-mq $SWIOTLB_OPTION
                sleep 5
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
            for i in "${!SWIOTLB_OPTIONS[@]}"
            do
                SWIOTLB_OPTION="${SWIOTLB_OPTIONS[$i]}"
                SWIOTLB_OPTION_TAG="${SWIOTLB_OPTIONS_TAGS[$i]}"
                just trace ${type_}_${SWIOTLB_OPTION_TAG}_redis 30 &
                inv vm.start --type ${type_} --size ${size} --virtio-nic --action="run-${action}" --virtio-nic-tap="tap_cvm" $SWIOTLB_OPTION
                sleep 5
                just trace ${type_}_vhost_${SWIOTLB_OPTION_TAG}_redis 30 &
                inv vm.start --type ${type_} --size ${size} --virtio-nic --action="run-${action}" --virtio-nic-tap="tap_cvm" --virtio-nic-vhost $SWIOTLB_OPTION
                sleep 5
                just trace ${type_}_mq_${SWIOTLB_OPTION_TAG}_redis 30 &
                inv vm.start --type ${type_} --size ${size} --virtio-nic --action="run-${action}" --virtio-nic-mtap="mtap_cvm" --virtio-nic-mq $SWIOTLB_OPTION
                sleep 5
                just trace ${type_}_vhost_mq_${SWIOTLB_OPTION_TAG}_redis 30 &
                inv vm.start --type ${type_} --size ${size} --virtio-nic --action="run-${action}" --virtio-nic-mtap="mtap_cvm" --virtio-nic-vhost --virtio-nic-mq $SWIOTLB_OPTION
                sleep 5
            done
        done
    done
done

for size in medium
do
    for type_ in "${VM[@]}"
    do
        for action in memtier-memcached
        do
            for i in "${!SWIOTLB_OPTIONS[@]}"
            do
                SWIOTLB_OPTION="${SWIOTLB_OPTIONS[$i]}"
                SWIOTLB_OPTION_TAG="${SWIOTLB_OPTIONS_TAGS[$i]}"
                just trace ${type_}_${SWIOTLB_OPTION_TAG}_memcached 30 &
                inv vm.start --type ${type_} --size ${size} --virtio-nic --action="run-${action}" --virtio-nic-tap="tap_cvm" $SWIOTLB_OPTION
                sleep 5
                just trace ${type_}_vhost_${SWIOTLB_OPTION_TAG}_memcached 30 &
                inv vm.start --type ${type_} --size ${size} --virtio-nic --action="run-${action}" --virtio-nic-tap="tap_cvm" --virtio-nic-vhost $SWIOTLB_OPTION
                sleep 5
                just trace ${type_}_mq_${SWIOTLB_OPTION_TAG}_memcached 30 &
                inv vm.start --type ${type_} --size ${size} --virtio-nic --action="run-${action}" --virtio-nic-mtap="mtap_cvm" --virtio-nic-mq $SWIOTLB_OPTION
                sleep 5
                just trace ${type_}_vhost_mq_${SWIOTLB_OPTION_TAG}_memcached 30 &
                inv vm.start --type ${type_} --size ${size} --virtio-nic --action="run-${action}" --virtio-nic-mtap="mtap_cvm" --virtio-nic-vhost --virtio-nic-mq $SWIOTLB_OPTION
                sleep 5
            done
        done
    done
done

for size in medium
do
    for type_ in "${VM[@]}"
    do
        for action in fio
        do
            for disk in ${DISKS}
            do  
                just trace ${type_}_fio 240 &
                inv vm.start --type ${type_} --size ${size} --virtio-blk /dev/${disk} --no-warn --action="run-${action}"  --fio-job "libaio"
                sleep 5
            done
        done
    done
done

for size in medium
do
    for type_ in "${VM[@]}"
    do
        for action in fio
        do
            for disk in $DISKS
            do
                for i in "${!SWIOTLB_OPTIONS[@]}"
                do
                    SWIOTLB_OPTION="${SWIOTLB_OPTIONS[$i]}"
                    SWIOTLB_OPTION_TAG="${SWIOTLB_OPTIONS_TAGS[$i]}"
                    just trace ${type_}_${SWIOTLB_OPTION_TAG}_fio 240 &
                    inv vm.start --type ${type_} --size ${size} --virtio-blk /dev/$disk --no-warn --action="run-${action}"  --fio-job "libaio" $SWIOTLB_OPTION
                    sleep 5
                done
            done
        done
    done
done