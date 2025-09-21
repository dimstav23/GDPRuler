#!/bin/bash

set -e

# Configuration
declare -A CONFIG=(
    [PROJECT_ROOT]="$(git rev-parse --show-toplevel 2>/dev/null)"
    [VM_CORES]="24"
    [VM_MEMORY]="32768"
    [MAX_WAIT_ATTEMPTS]="60"
    [TMP_DIR]="/tmp"
    [DB_DUMP_DIR]="/scratch/$(whoami)/gdpruler_fs/db_data"
    [CTL_DUMP_DIR]="/scratch/$(whoami)/gdpruler_fs/controller_data"
    [NODE_BIND]="numactl --cpunodebind=0 --membind=0"
    [NODE_BIND_CLIENT]="numactl --cpunodebind=1 --membind=1"
    [CVM_IP]="192.168.122.48"
    [SSH_OPTS]="-o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR"
)

declare -A PATHS=(
    [ROCKSDB_BIN]="${CONFIG[PROJECT_ROOT]}/controller/build/rocksdb_server"
    [ROCKSDB_SOCKET]="/tmp/rocksdb.sock"
    [REDIS_BIN]="${CONFIG[PROJECT_ROOT]}/KVs/redis/src/redis-server"
    [REDIS_SOCKET]="/tmp/redis.sock"
    [CLIENT]="${CONFIG[PROJECT_ROOT]}/scripts/client.py"
    [DIRECT_CLIENT]="${CONFIG[PROJECT_ROOT]}/scripts/direct_client.py"
    [GDPR_CONTROLLER]="${CONFIG[PROJECT_ROOT]}/scripts/GDPRuler.py"
    [PASSTHROUGH_CONTROLLER]="${CONFIG[PROJECT_ROOT]}/scripts/passthrough.py"
)

# Storage configuration
NVME_DEVICE="/dev/nvme1n1"
MOUNT_POINT="/scratch/dimitrios/gdpruler_fs"
DEVICE_IN_CVM="/dev/vda"
FILESYSTEM_TYPE="ext4"

# Global variables
failed_tests=""
server_connection=""
CVM_EXPECT_PID=""
CVM_READY=false
CVM_IP="${CONFIG[CVM_IP]}"
USE_DRAIN="false"

# Function to start the CVM using the listed expect script
boot_cvm() {
    local curr_dir=$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")
    local nvme_device="${NVME_DEVICE:-/dev/nvme1n1}"
    
    # Pass storage parameters to launch-qemu.sh
    local storage_opts="-nvme $nvme_device"

    # use -noiommu if you want to disable the iommu for performance reasons
    expect -c "
        log_user 0
        set timeout -1
        spawn sudo LD_LIBRARY_PATH=$::env(LD_LIBRARY_PATH) numactl ${CONFIG[NODE_BIND]} \
            bash ${curr_dir}/../CVM_setup/AMDSEV/launch-qemu.sh \
            -hda ${curr_dir}/../CVM_setup/images/gdpr.img \
            -sev-snp \
            -bridge virbr0 \
            -bios ${curr_dir}/../CVM_setup/firmware/gdpr \
            -smp ${CONFIG[VM_CORES]} \
            -mem ${CONFIG[VM_MEMORY]} \
            -vhost \
            $storage_opts \
            -log ${curr_dir}/cvm_boot.out

        expect \"login: \"
        send \"root\\r\"
        expect \"Password: \"
        send \"123456\\r\"
        expect \"# \"
        
        expect eof
    " &
    
    CVM_EXPECT_PID=$!
    echo "CVM started with PID: $CVM_EXPECT_PID"
    
    # Wait for CVM to be ready
    echo "Waiting for CVM to be ready..."
    sleep 5
    
    for ((attempt=1; attempt<=${CONFIG[MAX_WAIT_ATTEMPTS]}; attempt++)); do
        if ssh ${CONFIG[SSH_OPTS]} root@${CONFIG[CVM_IP]} "echo 'ready'" >/dev/null 2>&1; then
            CVM_READY=true
            echo "CVM is ready and accessible via SSH"
            # Prepare CVM storage
            prepare_storage_cvm ${DEVICE_IN_CVM} ${MOUNT_POINT} ${FILESYSTEM_TYPE}
            return 0
        fi
        sleep 2
    done
    
    echo "Failed to connect to CVM"
    return 1
}

# Function that shuts down the CVM and cleans up the qemu processes
shutdown_cvm() {
    echo -e "\e[31mShutting down CVM...\e[0m"

    # Kill the expect process
    if [ -n "$CVM_EXPECT_PID" ] && ps -p $CVM_EXPECT_PID > /dev/null 2>&1; then
        kill $CVM_EXPECT_PID 2>/dev/null || true
        sleep 2
        kill -9 $CVM_EXPECT_PID 2>/dev/null || true
    fi
    
    sudo pkill -f "launch-qemu.sh" 2>/dev/null || true
    sudo pkill -f qemu-system 2>/dev/null || true
    CVM_READY=false
}

# Helper function to execute a command within the CVM
execute_in_cvm() {
    local command="$1"
    local output_file="$2"
    
    if [ "$CVM_READY" != true ]; then
        echo "Error: CVM is not ready"
        return 1
    fi
    
    if [ -n "$output_file" ]; then
        ssh ${CONFIG[SSH_OPTS]} root@${CONFIG[CVM_IP]} "$command" > "$output_file" 2>&1
    else
        ssh ${CONFIG[SSH_OPTS]} root@${CONFIG[CVM_IP]} "$command"
    fi
}

# Helper function to execute a command within the CVM in the background
execute_in_cvm_background() {
    local command="$1"
    local output_file="$2"
    
    if [ "$CVM_READY" != true ]; then
        echo "Error: CVM is not ready"
        return 1
    fi
    
    echo "Executing in CVM background: $command"
    ssh ${CONFIG[SSH_OPTS]} root@${CONFIG[CVM_IP]} "nohup bash -c '$command' > $output_file 2>&1 &"
}

# Utility function to validate if an executable exists
validate_executable() {
    local executable="$1"
    local description="$2"
    
    if [ ! -f "$executable" ]; then
        echo "$description not found in $executable. Exiting..."
        exit 1
    fi
}

# Function to wait for TCP activation
wait_for_tcp_activation() {
    local ip_address="$1"
    local port="$2"

    for ((attempt=1; attempt<=${CONFIG[MAX_WAIT_ATTEMPTS]}; attempt++)); do
        if nc -z "$ip_address" "$port" &> /dev/null; then
            return 0
        fi
        sleep 1
    done
    
    echo "Timeout: $ip_address:$port did not become active within ${CONFIG[MAX_WAIT_ATTEMPTS]} seconds."
    exit 1
}

# Function to wait for UNIX socket activation
wait_for_unix_socket_activation() {
    local socket_path="$1"
    
    for ((attempt=1; attempt<=${CONFIG[MAX_WAIT_ATTEMPTS]}; attempt++)); do
        if [ -S "$socket_path" ]; then
            return 0
        fi
        sleep 1
    done
    
    echo "Timeout: $socket_path did not become active within ${CONFIG[MAX_WAIT_ATTEMPTS]} seconds."
    exit 1
}

# Function to select the server type and run it
run_server() {
    local db="$1"
    local environment="$2"  # "native" or "CVM"
    local db_address="$3"
    local port="$4"
    local db_dir="$5"
    local output_file="$6"
    
    case "$environment" in
        "native")
            case "$db" in
                "rocksdb") run_rocksdb_native "$db_address" "$port" "$db_dir" "$output_file" ;;
                "redis") run_redis_native "$db_address" "$port" "$db_dir" "$output_file" ;;
            esac
            ;;
        "CVM")
            run_server_CVM_direct "$db" "$db_address" "$port" "$db_dir" "$output_file"
            ;;
    esac
}

# Function that runs a DB server inside the CVM - the connection is ALWAYS via TCP socket
run_server_CVM_direct() {
    local db="$1"
    local db_address="$2"
    local port="$3"
    local db_dir="$4"
    local output_file="$5"

    echo "Starting $db server in CVM"
    
    case "$db" in
        "rocksdb")
            local server_cmd="/root/GDPRuler/controller/build/rocksdb_server $port $db_dir"
            execute_in_cvm_background "$server_cmd" "$output_file"
            ;;
        "redis")
            local server_cmd="/root/GDPRuler/KVs/redis/src/redis-server --port $port --dir $db_dir --protected-mode no"
            execute_in_cvm_background "$server_cmd" "$output_file"
            ;;
    esac
    
    # Wait for server to be ready
    for ((attempt=1; attempt<=${CONFIG[MAX_WAIT_ATTEMPTS]}; attempt++)); do
        # Check for TCP port
        if execute_in_cvm "netstat -ln | grep :$port" > /dev/null 2>&1; then
            echo "Database server $db started successfully on port $port in CVM"
            return 0
        fi
        sleep 2
    done
    
    echo "Failed to start database server $db in CVM"
    return 1
}

# Function that runs a RocksDB server natively
run_rocksdb_native() {
    local db_address="$1"
    local port="$2"
    local db_dir="$3"
    local output_file="$4"
    
    validate_executable "${PATHS[ROCKSDB_BIN]}" "RocksDB server"
    
    if [[ $port == "0" ]]; then
        echo "Starting rocksdb server: ${CONFIG[NODE_BIND]} ${PATHS[ROCKSDB_BIN]} --unix ${PATHS[ROCKSDB_SOCKET]} $db_dir > $output_file"
        ${CONFIG[NODE_BIND]} "${PATHS[ROCKSDB_BIN]}" --unix "${PATHS[ROCKSDB_SOCKET]}" "$db_dir" > "$output_file" &
        wait_for_unix_socket_activation "${PATHS[ROCKSDB_SOCKET]}"
    else
        echo "Starting rocksdb server: ${CONFIG[NODE_BIND]} ${PATHS[ROCKSDB_BIN]} $port $db_dir > $output_file"
        ${CONFIG[NODE_BIND]} "${PATHS[ROCKSDB_BIN]}" "$port" "$db_dir" > "$output_file" &
        wait_for_tcp_activation "localhost" "$port"
    fi
}

# Function that runs a Redis server natively
run_redis_native() {
    local db_address="$1"
    local port="$2"
    local db_dir="$3"
    local output_file="$4"
    
    validate_executable "${PATHS[REDIS_BIN]}" "Redis server"
    
    if [[ $port == "0" ]]; then
        echo "Starting redis server: ${CONFIG[NODE_BIND]} ${PATHS[REDIS_BIN]} --port $port --unixsocket ${PATHS[REDIS_SOCKET]} --unixsocketperm 700 --dir $db_dir --protected-mode no > $output_file"
        ${CONFIG[NODE_BIND]} "${PATHS[REDIS_BIN]}" --port "$port" --unixsocket "${PATHS[REDIS_SOCKET]}" --unixsocketperm 700 --dir "$db_dir" --protected-mode no > "$output_file" &
        wait_for_unix_socket_activation "${PATHS[REDIS_SOCKET]}"
    else
        echo "Starting redis server: ${CONFIG[NODE_BIND]} ${PATHS[REDIS_BIN]} --port $port --dir $db_dir --protected-mode no > $output_file"
        ${CONFIG[NODE_BIND]} "${PATHS[REDIS_BIN]}" --port "$port" --dir "$db_dir" --protected-mode no > "$output_file" &
        wait_for_tcp_activation "localhost" "$port"
    fi
}

# Function to select the controller type and run it
run_controller() {
    local controller_type="$1"  # "gdpr", "passthrough"
    local environment="$2"      # "native", "CVM"
    local controller_address="$3"
    local controller_port="$4"
    local db="$5"
    local db_address="$6"
    local db_dir="$7"
    local ctl_dir="$8"
    local ctl_output_file="$9"
    local server_output_file="${10}"

    case "$environment" in
        "native")
            run_controller_native "$controller_type" "$controller_address" "$controller_port" "$db" "$db_address" "$db_dir" "$ctl_dir" "$ctl_output_file"
            ;;
        "CVM")
            run_controller_CVM "$controller_type" "$controller_address" "$controller_port" "$db" "$db_address" "$db_dir" "$ctl_dir" "$ctl_output_file" "$server_output_file"
            ;;
    esac
}

# Function to run the controller natively
run_controller_native() {
    local controller_type="$1"
    local controller_address="$2"
    local controller_port="$3"
    local db="$4"
    local db_address="$5"
    local db_dir="$6"
    local ctl_dir="$7"
    local output_file="$8"
    
    local controller_bin=""
    case "$controller_type" in
        "gdpr") controller_bin="${PATHS[GDPR_CONTROLLER]}" ;;
        "passthrough") controller_bin="${PATHS[PASSTHROUGH_CONTROLLER]}" ;;
    esac
    
    validate_executable "$controller_bin" "Controller"
    
    local ctl_cmd="$controller_bin --db $db --controller_address $controller_address --controller_port $controller_port"
    
    if [[ $controller_type == "gdpr" ]]; then
        ctl_cmd="$ctl_cmd --logpath $ctl_dir"
    fi
    
    if [[ $server_connection != "UNIX" ]]; then
        ctl_cmd="$ctl_cmd --db_address $db_address"
    fi
    
    echo "Starting the $controller_type controller: ${CONFIG[NODE_BIND]} python3 $ctl_cmd > $output_file"
    ${CONFIG[NODE_BIND]} python3 $ctl_cmd > "$output_file" &
    wait_for_tcp_activation "localhost" "$controller_port"
}

# Starts the DB inside the CVM and runs the controller - the connection is ALWAYS via Unix socket
run_controller_CVM() {
    local controller_type="$1"
    local controller_address="$2"
    local controller_port="$3"
    local db="$4"
    local db_address="$5"
    local db_dir="$6"
    local ctl_dir="$7"
    local ctl_output_file="$8"
    local server_output_file="$9"

    echo "Starting the $controller_type controller in CVM"
    
    # Start database server in CVM
    case "$db" in
        "rocksdb")
            local db_socket=${PATHS[ROCKSDB_SOCKET]}
            local server_cmd="/root/GDPRuler/controller/build/rocksdb_server --unix $db_socket $db_dir"
            execute_in_cvm_background "$server_cmd" "$server_output_file"
            ;;
        "redis")
            local db_socket=${PATHS[REDIS_SOCKET]}
            local server_cmd="/root/GDPRuler/KVs/redis/src/redis-server --port 0 --unixsocket $db_socket --unixsocketperm 700 --dir $db_dir --protected-mode no"
            execute_in_cvm_background "$server_cmd" "$server_output_file"
            ;;
    esac
    
    # Wait for database socket
    for ((attempt=1; attempt<=${CONFIG[MAX_WAIT_ATTEMPTS]}; attempt++)); do
        # Check for socket file
        if execute_in_cvm "[ -S $db_socket ]"; then
            echo "Database server $db started successfully with UNIX socket in CVM"
            break
        fi
        sleep 1
    done

    if [[ $attempt -gt ${CONFIG[MAX_WAIT_ATTEMPTS]} ]]; then
        echo "Error: Database server $db failed to start within the expected time."
        return 1
    fi

    # Start controller in CVM
    local controller_script=""
    case "$controller_type" in
        "gdpr") controller_script="/root/GDPRuler/scripts/GDPRuler.py" ;;
        "passthrough") controller_script="/root/GDPRuler/scripts/passthrough.py" ;;
    esac
    
    local controller_cmd="python3 $controller_script --db $db --controller_address $controller_address --controller_port $controller_port"
    if [[ $controller_type == "gdpr" ]]; then
        controller_cmd="$controller_cmd --logpath $ctl_dir"
    fi

    echo "Starting $controller_type controller in CVM: $controller_cmd"
    execute_in_cvm_background "$controller_cmd" "$ctl_output_file"
    wait_for_tcp_activation "$controller_address" "$controller_port"
}

# Function that runs a client with the given configurationW
run_client() {
    local client_type="$1"  # "direct" or "controller"
    local workload="$2"
    local n_clients="$3"
    local address="$4"
    local port="$5"
    local output_file="$6"
    local extra_args="$7"
    local use_drain="$8"
    
    local client_bin=""
    local client_cmd=""
    
    case "$client_type" in
        "direct")
            client_bin="${PATHS[DIRECT_CLIENT]}"
            validate_executable "$client_bin" "Direct client"
            client_cmd="$client_bin --db $extra_args --db_address $address --workload $workload --clients $n_clients"
            ;;
        "controller")
            client_bin="${PATHS[CLIENT]}"
            validate_executable "$client_bin" "Client"
            client_cmd="$client_bin --workload $workload --clients $n_clients --address $address --port $port --config $extra_args"
            if [[ "$use_drain" == "true" ]]; then
                client_cmd="$client_cmd --drain"
            fi
            ;;
    esac
    
    echo "Starting the client(s): ${CONFIG[NODE_BIND_CLIENT]} $client_cmd > $output_file"
    ${CONFIG[NODE_BIND_CLIENT]} python3 $client_cmd > "$output_file"
    local exit_code=$?
    
    if [ $exit_code -eq 0 ]; then
        echo -e "\e[32m✓ Client process completed successfully\e[0m"
    else
        echo -e "\e[31m✗ Client process failed with exit code $exit_code\e[0m"
    fi
    
    return $exit_code
}

# Function to prepare the experiment environment
prepare_experiment() {
    local result_file="$1"

    mkdir -p "${CONFIG[TMP_DIR]}"
    rm -rf "${CONFIG[DB_DUMP_DIR]}" "${CONFIG[CTL_DUMP_DIR]}"
    mkdir -p "${CONFIG[DB_DUMP_DIR]}" "${CONFIG[CTL_DUMP_DIR]}"
    
    if [ ! -f "$result_file" ]; then
        install -D -m 644 /dev/null "$result_file"
        echo "workload,controller,db,n_clients,elapsed_time (s),avg_latency (s),ctl_files_count,ctl_files_size_mb,db_files_count,db_files_size_mb" >> "$result_file"
    fi
}

# Storage device preparation on the host
prepare_storage_host() {
    local device="$1"
    local mount_point="$2"
    local fs_type="$3"
    
    echo "Preparing host storage: device=$device, mount_point=$mount_point, fs_type=$fs_type"
    
    # Check if already mounted with correct filesystem
    if mountpoint -q "$mount_point" 2>/dev/null; then
        local current_fs=$(findmnt -n -o FSTYPE "$mount_point" 2>/dev/null)
        
        if [[ "$current_fs" == "$fs_type" ]]; then
            echo "Storage already mounted with correct filesystem ($current_fs) at $mount_point"
            return 0
        else
            echo "Wrong filesystem type ($current_fs), unmounting..."
            sudo umount "$mount_point" || true
        fi
    fi
    
    # Create filesystem and mount
    echo "Creating $fs_type filesystem on $device"
    sudo mkfs.$fs_type -F "$device"
    
    sudo mkdir -p "$mount_point"
    sudo mount "$device" "$mount_point"
    sudo chown -R $(whoami):$(id -gn $(whoami)) "$mount_point"
    
    echo "Storage prepared and mounted at $mount_point"
}

# Storage device cleanup on the host
cleanup_storage_host() {
    local mount_point="$1"
    
    if mountpoint -q "$mount_point" 2>/dev/null; then
        echo "Unmounting storage at $mount_point"
        sudo umount -f "$mount_point" || true
    fi
}

# Storage device preparation in the CVM
prepare_storage_cvm() {
    local device="$1"
    local mount_point="$2"  
    local fs_type="$3"
    
    echo "Preparing CVM storage at $mount_point"
    
    # Check if storage device exists in CVM
    if ! execute_in_cvm "[ -b $device ]"; then
        echo "Error: Storage device $device not available in CVM"
        return 1
    fi
    
    execute_in_cvm "mkdir -p $mount_point"
    
    # Check if already mounted with correct filesystem
    if execute_in_cvm "mountpoint -q $mount_point 2>/dev/null"; then
        local current_fs=$(execute_in_cvm "findmnt -n -o FSTYPE $mount_point 2>/dev/null")
        
        if [[ "$current_fs" == "$fs_type" ]]; then
            echo "CVM storage already mounted with correct filesystem ($current_fs)"
            return 0
        else
            echo "Wrong filesystem type in CVM ($current_fs), unmounting..."
            execute_in_cvm "umount $mount_point" || true
        fi
    fi
    
    # Create filesystem on the device
    echo "virtio-blk mode: Creating $fs_type filesystem in CVM"
    execute_in_cvm "mkfs.$fs_type -F $device"
    
    # Mount the filesystem
    echo "Mounting filesystem in CVM"
    execute_in_cvm "mount $device $mount_point"
    
    echo "CVM storage ready at $mount_point"
}

# Function to collect results from the client output
collect_results() {
    local workload="$1"
    local controller="$2"
    local db="$3"
    local n_clients="$4"
    local results_file="$5"
    local environment="$6"  # "native" or "CVM"
    
    local elapsed_time=$(grep "Elapsed time: " "${CONFIG[TMP_DIR]}/clients.txt" | awk '{print $3}')
    local avg_latency=$(grep "Average Latency: " "${CONFIG[TMP_DIR]}/clients.txt" | awk '{print $3}')
    
    if [ -z "$avg_latency" ]; then
        failed_tests="$failed_tests $workload,controller=$controller,$db,clients=$n_clients"
    else
        # Collect storage metrics before cleanup
        local storage_metrics=$(collect_storage_metrics "$environment" "${CONFIG[CTL_DUMP_DIR]}" "${CONFIG[DB_DUMP_DIR]}")

        echo "$workload,$controller,$db,$n_clients,$elapsed_time,$avg_latency,$storage_metrics" >> "$results_file"
        echo -e "\e[32m✓ Results for $workload, controller=$controller, db=$db, clients=$n_clients: time=$elapsed_time, latency=$avg_latency, storage=$storage_metrics\e[0m"
    fi
}

# Function to collect storage metrics before cleanup
collect_storage_metrics() {
    local environment="$1"  # "native" or "CVM"
    local ctl_dir="$2"
    local db_dir="$3"
    
    local ctl_files_count=0
    local ctl_files_size_mb=0
    local db_files_count=0  
    local db_files_size_mb=0
    
    if [[ "$environment" == "native" ]]; then
        # Collect metrics on host
        if [[ -d "$ctl_dir" ]]; then
            ctl_files_count=$(find "$ctl_dir" -type f 2>/dev/null | wc -l)
            local ctl_size_bytes=$(du -sb "$ctl_dir" 2>/dev/null | cut -f1)
            ctl_files_size_mb=$(echo "scale=2; $ctl_size_bytes / 1024 / 1024" | bc -l 2>/dev/null || echo "0")
        fi
        
        if [[ -d "$db_dir" ]]; then
            # Send SIGINT to flush and close the DB properly
            kill -INT $(pgrep -f rocksdb_server) 2>/dev/null || true
            kill -INT $(pgrep -f redis-server) 2>/dev/null || true
            sleep 5
            db_files_count=$(find "$db_dir" -type f 2>/dev/null | wc -l)
            local db_size_bytes=$(du -sb "$db_dir" 2>/dev/null | cut -f1)
            db_files_size_mb=$(echo "scale=2; $db_size_bytes / 1024 / 1024" | bc -l 2>/dev/null || echo "0")
        fi
    else
        set +e # Disable exit on error for CVM commands
        # Collect metrics in CVM
        if execute_in_cvm "[ -d '$ctl_dir' ]" 2>/dev/null; then
            ctl_files_count=$(execute_in_cvm "find '$ctl_dir' -type f 2>/dev/null | wc -l" 2>/dev/null || echo "0")
            local ctl_size_bytes=$(execute_in_cvm "du -sb '$ctl_dir' 2>/dev/null | cut -f1" 2>/dev/null || echo "0")
            ctl_files_size_mb=$(echo "scale=2; $ctl_size_bytes / 1024 / 1024" | bc -l 2>/dev/null || echo "0")
        fi
        
        if execute_in_cvm "[ -d '$db_dir' ]" 2>/dev/null; then
            # Send SIGINT to flush and close the DB properly
            execute_in_cvm "kill -INT \$(pgrep -f rocksdb_server) 2>/dev/null || true"
            execute_in_cvm "kill -INT \$(pgrep -f redis-server) 2>/dev/null || true"
            sleep 5
            db_files_count=$(execute_in_cvm "find '$db_dir' -type f 2>/dev/null | wc -l" 2>/dev/null || echo "0")
            local db_size_bytes=$(execute_in_cvm "du -sb '$db_dir' 2>/dev/null | cut -f1" 2>/dev/null || echo "0")
            db_files_size_mb=$(echo "scale=2; $db_size_bytes / 1024 / 1024" | bc -l 2>/dev/null || echo "0")
        fi
        set -e # Re-enable exit on error
    fi
    
    # Return the metrics as a comma-separated string
    echo "$ctl_files_count,$ctl_files_size_mb,$db_files_count,$db_files_size_mb"
}

# Function that cleans up processes and files from previous experiments in CVM via SSH
cleanup_cvm_previous_experiment() {
    if [ "$CVM_READY" = true ]; then
        echo "Stopping processes from previous experiments in the CVM..."
        set +e
        execute_in_cvm "kill \$(pgrep -f native_controller) 2>/dev/null || true"
        execute_in_cvm "kill \$(pgrep -f gdpr_controller) 2>/dev/null || true"
        execute_in_cvm "kill \$(pgrep -f rocksdb_server) 2>/dev/null || true"
        execute_in_cvm "kill \$(pgrep -f redis-server) 2>/dev/null || true"

        for ((attempt=1; attempt<=${CONFIG[MAX_WAIT_ATTEMPTS]}; attempt++)); do
            local remaining_processes=$(execute_in_cvm "ps aux | grep -E '(rocksdb_server|redis-server|native_controller|gdpr_controller)' | grep -v grep | grep -v ssh | wc -l")
            if [ "$remaining_processes" = "0" ]; then
                echo "All processes terminated successfully"
                break
            fi
            echo $remaining_processes "process(es) still running in CVM, waiting..."       
            sleep 1
        done

        if [ "$attempt" -gt "${CONFIG[MAX_WAIT_ATTEMPTS]}" ]; then
            echo "Warning: Some processes may still be running after ${CONFIG[MAX_WAIT_ATTEMPTS]} seconds"
            execute_in_cvm "ps aux | grep -E '(rocksdb|redis|native_controller|gdpr_controller)' | grep -v grep || true"
            exit 1
        fi

        echo "Cleaning up files in CVM..."
        execute_in_cvm "rm -rf ${PATHS[REDIS_SOCKET]} ${PATHS[ROCKSDB_SOCKET]}"
        execute_in_cvm "rm -rf ${CONFIG[TMP_DIR]}/server.txt ${CONFIG[TMP_DIR]}/controller.txt"
        execute_in_cvm "rm -rf ${CONFIG[DB_DUMP_DIR]}/* && mkdir -p ${CONFIG[DB_DUMP_DIR]}"
        execute_in_cvm "rm -rf ${CONFIG[CTL_DUMP_DIR]}/* && mkdir -p ${CONFIG[CTL_DUMP_DIR]}"
        set -e
        echo "All cleanup operations completed."
    fi
}

# Function to clean up processes and files on the host
cleanup_host() {
    local controller_address="$1"
    local controller_port="$2"
    local db="$3"
    local db_address="$4"
    local db_port="$5"
    
    echo "Stopping processes from previous experiments on the host..."
    kill $(pgrep -f native_controller) 2>/dev/null || true
    kill $(pgrep -f gdpr_controller) 2>/dev/null || true
    kill $(pgrep -f rocksdb_server) 2>/dev/null || true
    kill $(pgrep -f redis-server) 2>/dev/null || true
    
    for ((attempt=1; attempt<=${CONFIG[MAX_WAIT_ATTEMPTS]}; attempt++)); do
        local remaining_processes=$(ps aux | grep -E '(rocksdb_server|redis-server|native_controller|gdpr_controller)' | grep -v grep | grep -v ssh | wc -l)
        if [ "$remaining_processes" = "0" ]; then
            echo "All processes terminated successfully"
            break
        fi
        echo $remaining_processes "processes still running on the host, waiting..."       
        sleep 1
    done
    
    echo "Cleaning up files on the host..."
    rm -rf "${PATHS[REDIS_SOCKET]}" "${PATHS[ROCKSDB_SOCKET]}"
    rm -rf "${CONFIG[TMP_DIR]}"/server.txt "${CONFIG[TMP_DIR]}"/controller.txt "${CONFIG[TMP_DIR]}"/clients.txt
    rm -rf "${CONFIG[DB_DUMP_DIR]}" "${CONFIG[CTL_DUMP_DIR]}"
    
    echo "All cleanup operations completed."
}

# Function to clean up processes and files after an experiment
cleanup() {
    local controller_address="$1"
    local controller_port="$2"
    local db="$3"
    local db_address="$4"
    local db_port="$5"
    
    if [ "$CVM_READY" = true ]; then
        # Clean up processes in CVM
        cleanup_cvm_previous_experiment
    else
        # Clean up processes and files on the host
        cleanup_host "$controller_address" "$controller_port" "$db" "$db_address" "$db_port"
    fi
}

# Unified experiment function
run_experiment() {
    local experiment_type="$1"  # "native_direct", "native_ctl", "CVM_direct", "CVM_passthrough", "CVM_gdpr"
    local n_clients="$2"
    local workload="$3"
    local db="$4"
    local db_address="$5"
    local db_port="$6"
    local results_csv_file="$7"
    local use_drain="$8"
    shift 8
        
    local db_address_formatted="${db_address}:${db_port}"
    if [[ $db == "redis" ]]; then
        db_address_formatted="tcp://${db_address_formatted}"
    fi
    
    # Cleanup to get rid of leftovers from previous experiments
    cleanup "$controller_address" "$controller_port" "$db" "$db_address" "$db_port"
    # And prepare the experiment environment
    prepare_experiment "$results_csv_file"

    case "$experiment_type" in
        "native_direct")
            run_server "$db" "native" "$db_address" "$db_port" "${CONFIG[DB_DUMP_DIR]}" "${CONFIG[TMP_DIR]}/server.txt"
            run_client "direct" "$workload" "$n_clients" "$db_address_formatted" "" "${CONFIG[TMP_DIR]}/clients.txt" "$db" "$use_drain"
            collect_results "$workload" "direct" "$db" "$n_clients" "$results_csv_file" "native"
            ;;
        "native_ctl")
            local controller="$1"
            local controller_address="$2"
            local controller_port="$3"
            local config="$4"
            
            run_server "$db" "native" "$db_address" "$db_port" "${CONFIG[DB_DUMP_DIR]}" "${CONFIG[TMP_DIR]}/server.txt"
            run_controller "$controller" "native" "$controller_address" "$controller_port" "$db" "$db_address_formatted" "${CONFIG[DB_DUMP_DIR]}" "${CONFIG[CTL_DUMP_DIR]}" "${CONFIG[TMP_DIR]}/controller.txt" ""
            run_client "controller" "$workload" "$n_clients" "$controller_address" "$controller_port" "${CONFIG[TMP_DIR]}/clients.txt" "$config" "$use_drain"
            collect_results "$workload" "$controller" "$db" "$n_clients" "$results_csv_file" "native"
            ;;
        "CVM_direct")
            run_server "$db" "CVM" "$db_address" "$db_port" "${CONFIG[DB_DUMP_DIR]}" "${CONFIG[TMP_DIR]}/server.txt"
            run_client "direct" "$workload" "$n_clients" "$db_address_formatted" "" "${CONFIG[TMP_DIR]}/clients.txt" "$db" "$use_drain"
            collect_results "$workload" "direct" "$db" "$n_clients" "$results_csv_file" "CVM"
            ;;
        "CVM_passthrough"|"CVM_gdpr")
            local controller_type="${experiment_type#CVM_}"
            local controller_address="$1"
            local controller_port="$2"
            local config="$3"
            
            run_controller "$controller_type" "CVM" "$controller_address" "$controller_port" "$db" "$db_address" "${CONFIG[DB_DUMP_DIR]}" "${CONFIG[CTL_DUMP_DIR]}" "${CONFIG[TMP_DIR]}/controller.txt" "${CONFIG[TMP_DIR]}/server.txt"
            run_client "controller" "$workload" "$n_clients" "$controller_address" "$controller_port" "${CONFIG[TMP_DIR]}/clients.txt" "$config" "$use_drain"
            collect_results "$workload" "$controller_type" "$db" "$n_clients" "$results_csv_file" "CVM"
            ;;
    esac
    # Cleanup phase
    cleanup "$controller_address" "$controller_port" "$db" "$db_address" "$db_port"
}

# Function to print a summary of the results
print_summary() {
    if [ -n "$failed_tests" ]; then
        echo -e "\e[31mThe following tests failed:\e[0m"
        for test in $failed_tests; do
            echo -e "\e[31m$test\e[0m"
        done
    else
        echo -e "\e[32mAll tests were successful :)\e[0m"
    fi
}
