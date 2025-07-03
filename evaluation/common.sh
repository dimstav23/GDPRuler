#!/bin/bash

set -e

# Configuration
declare -A CONFIG=(
    [PROJECT_ROOT]="$(git rev-parse --show-toplevel 2>/dev/null)"
    [VM_CORES]="16"
    [VM_MEMORY]="16384"
    [MAX_WAIT_ATTEMPTS]="60"
    [TMP_DIR]="/tmp"
    [DB_DUMP_DIR]="/scratch/$(whoami)/data"
    [NODE_BIND]="numactl --cpunodebind=0 --membind=0"
)

declare -A PATHS=(
    [ROCKSDB_BIN]="${CONFIG[PROJECT_ROOT]}/controller/build/rocksdb_server"
    [ROCKSDB_SOCKET]="/tmp/rocksdb.sock"
    [REDIS_BIN]="${CONFIG[PROJECT_ROOT]}/KVs/redis/src/redis-server"
    [REDIS_SOCKET]="/tmp/redis.sock"
    [GDPR_EXPECT]="${CONFIG[PROJECT_ROOT]}/evaluation/VM/gdpr.expect"
    [PASSTHROUGH_EXPECT]="${CONFIG[PROJECT_ROOT]}/evaluation/VM/passthrough.expect"
    [SERVER_EXPECT]="${CONFIG[PROJECT_ROOT]}/evaluation/VM/direct.expect"
    [CLIENT]="${CONFIG[PROJECT_ROOT]}/scripts/client.py"
    [DIRECT_CLIENT]="${CONFIG[PROJECT_ROOT]}/scripts/direct_client.py"
    [GDPR_CONTROLLER]="${CONFIG[PROJECT_ROOT]}/scripts/GDPRuler.py"
    [PASSTHROUGH_CONTROLLER]="${CONFIG[PROJECT_ROOT]}/scripts/passthrough.py"
)

# Global variables
failed_tests=""
server_connection=""

# Utility functions
validate_executable() {
    local executable="$1"
    local description="$2"
    
    if [ ! -f "$executable" ]; then
        echo "$description not found in $executable. Exiting..."
        exit 1
    fi
}

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

wait_for_tcp_shutdown() {
    local ip_address="$1"
    local port="$2"
    
    for ((attempt=1; attempt<=${CONFIG[MAX_WAIT_ATTEMPTS]}; attempt++)); do
        if ! nc -z "$ip_address" "$port" &> /dev/null; then
            return 0
        fi
        sleep 1
    done
    
    echo "Timeout: $ip_address:$port did not become inactive within ${CONFIG[MAX_WAIT_ATTEMPTS]} seconds."
    exit 1
}

# Unified server functions
run_server() {
    local db="$1"
    local environment="$2"  # "native" or "CVM"
    local db_address="$3"
    local port="$4"
    local log_dir="$5"
    local output_file="$6"
    
    case "$environment" in
        "native")
            case "$db" in
                "rocksdb") run_rocksdb_native "$db_address" "$port" "$log_dir" "$output_file" ;;
                "redis") run_redis_native "$db_address" "$port" "$log_dir" "$output_file" ;;
            esac
            ;;
        "CVM")
            echo "Starting the $db server in a CVM"
            expect "${PATHS[SERVER_EXPECT]}" "$db" "${CONFIG[VM_CORES]}" "${CONFIG[VM_MEMORY]}" "$port" "$log_dir" "$output_file" &
            wait_for_tcp_activation "$db_address" "$port"
            ;;
    esac
}

run_rocksdb_native() {
    local db_address="$1"
    local port="$2"
    local log_dir="$3"
    local output_file="$4"
    
    validate_executable "${PATHS[ROCKSDB_BIN]}" "RocksDB server"
    
    if [[ $port == "0" ]]; then
        echo "Starting rocksdb server: ${CONFIG[NODE_BIND]} ${PATHS[ROCKSDB_BIN]} --unix ${PATHS[ROCKSDB_SOCKET]} $log_dir > $output_file"
        ${CONFIG[NODE_BIND]} "${PATHS[ROCKSDB_BIN]}" --unix "${PATHS[ROCKSDB_SOCKET]}" "$log_dir" > "$output_file" &
        wait_for_unix_socket_activation "${PATHS[ROCKSDB_SOCKET]}"
    else
        echo "Starting rocksdb server: ${CONFIG[NODE_BIND]} ${PATHS[ROCKSDB_BIN]} $port $log_dir > $output_file"
        ${CONFIG[NODE_BIND]} "${PATHS[ROCKSDB_BIN]}" "$port" "$log_dir" > "$output_file" &
        wait_for_tcp_activation "localhost" "$port"
    fi
}

run_redis_native() {
    local db_address="$1"
    local port="$2"
    local log_dir="$3"
    local output_file="$4"
    
    validate_executable "${PATHS[REDIS_BIN]}" "Redis server"
    
    if [[ $port == "0" ]]; then
        echo "Starting redis server: ${CONFIG[NODE_BIND]} ${PATHS[REDIS_BIN]} --port $port --unixsocket ${PATHS[REDIS_SOCKET]} --unixsocketperm 700 --dir $log_dir --protected-mode no > $output_file"
        ${CONFIG[NODE_BIND]} "${PATHS[REDIS_BIN]}" --port "$port" --unixsocket "${PATHS[REDIS_SOCKET]}" --unixsocketperm 700 --dir "$log_dir" --protected-mode no > "$output_file" &
        wait_for_unix_socket_activation "${PATHS[REDIS_SOCKET]}"
    else
        echo "Starting redis server: ${CONFIG[NODE_BIND]} ${PATHS[REDIS_BIN]} --port $port --dir $log_dir --protected-mode no > $output_file"
        ${CONFIG[NODE_BIND]} "${PATHS[REDIS_BIN]}" --port "$port" --dir "$log_dir" --protected-mode no > "$output_file" &
        wait_for_tcp_activation "localhost" "$port"
    fi
}

# Unified controller functions
run_controller() {
    local controller_type="$1"  # "gdpr", "passthrough"
    local environment="$2"      # "native", "CVM"
    local controller_address="$3"
    local controller_port="$4"
    local db="$5"
    local db_address="$6"
    local log_path="$7"
    local ctl_output_file="$8"
    local server_output_file="$9"
    
    case "$environment" in
        "native")
            run_controller_native "$controller_type" "$controller_address" "$controller_port" "$db" "$db_address" "$log_path" "$ctl_output_file"
            ;;
        "CVM")
            run_controller_CVM "$controller_type" "$controller_address" "$controller_port" "$db" "$db_address" "$log_path" "$ctl_output_file" "$server_output_file"
            ;;
    esac
}

run_controller_native() {
    local controller_type="$1"
    local controller_address="$2"
    local controller_port="$3"
    local db="$4"
    local db_address="$5"
    local log_path="$6"
    local output_file="$7"
    
    local controller_bin=""
    case "$controller_type" in
        "gdpr") controller_bin="${PATHS[GDPR_CONTROLLER]}" ;;
        "passthrough") controller_bin="${PATHS[PASSTHROUGH_CONTROLLER]}" ;;
    esac
    
    validate_executable "$controller_bin" "Controller"
    
    local ctl_cmd="$controller_bin --db $db --controller_address $controller_address --controller_port $controller_port"
    
    if [[ $controller_type == "gdpr" ]]; then
        ctl_cmd="$ctl_cmd --logpath $log_path"
    fi
    
    if [[ $server_connection != "UNIX" ]]; then
        ctl_cmd="$ctl_cmd --db_address $db_address"
    fi
    
    echo "Starting the $controller_type controller: ${CONFIG[NODE_BIND]} python3 $ctl_cmd > $output_file"
    ${CONFIG[NODE_BIND]} python3 $ctl_cmd > "$output_file" &
    wait_for_tcp_activation "localhost" "$controller_port"
}

run_controller_CVM() {
    local controller_type="$1"
    local controller_address="$2"
    local controller_port="$3"
    local db="$4"
    local db_address="$5"
    local log_path="$6"
    local ctl_output_file="$7"
    local server_output_file="$8"
    
    echo "Starting the $controller_type controller in a CVM"
    
    local expect_script=""
    case "$controller_type" in
        "gdpr") expect_script="${PATHS[GDPR_EXPECT]}" ;;
        "passthrough") expect_script="${PATHS[PASSTHROUGH_EXPECT]}" ;;
    esac
    
    expect "$expect_script" "$db" "${CONFIG[VM_CORES]}" "${CONFIG[VM_MEMORY]}" "$db_address" "$controller_address" "$controller_port" "$log_path" "$server_output_file" "$ctl_output_file" &
    wait_for_tcp_activation "$controller_address" "$controller_port"
}

# Client functions
run_client() {
    local client_type="$1"  # "direct" or "controller"
    local workload="$2"
    local n_clients="$3"
    local address="$4"
    local port="$5"
    local output_file="$6"
    local extra_args="$7"
    
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
            ;;
    esac
    
    echo "Starting the client(s): $client_cmd"
    ${CONFIG[NODE_BIND]} python3 $client_cmd > "$output_file"
    return $?
}

# Experiment management
prepare_experiment() {
    local result_file="$1"
    
    mkdir -p "${CONFIG[TMP_DIR]}"
    rm -rf "${CONFIG[DB_DUMP_DIR]}"
    mkdir -p "${CONFIG[DB_DUMP_DIR]}"
    
    if [ ! -f "$result_file" ]; then
        install -D -m 644 /dev/null "$result_file"
        echo "workload,controller,db,n_clients,elapsed_time (s),avg_latency (s)" >> "$result_file"
    fi
}

collect_results() {
    local workload="$1"
    local controller="$2"
    local db="$3"
    local n_clients="$4"
    local results_file="$5"
    
    local elapsed_time=$(grep "Elapsed time: " "${CONFIG[TMP_DIR]}/clients.txt" | awk '{print $3}')
    local avg_latency=$(grep "Average Latency: " "${CONFIG[TMP_DIR]}/clients.txt" | awk '{print $3}')
    
    if [ -z "$avg_latency" ]; then
        failed_tests="$failed_tests $workload,controller=$controller,$db,clients=$n_clients"
    else
        echo "$workload,$controller,$db,$n_clients,$elapsed_time,$avg_latency" >> "$results_file"
    fi
}

cleanup() {
    local controller_address="$1"
    local controller_port="$2"
    local db="$3"
    local db_address="$4"
    local db_port="$5"
    
    echo "Stopping all relevant processes"
    sudo kill -SIGINT $(pgrep -f qemu) 2>/dev/null || true
    kill $(pgrep -f native_controller) 2>/dev/null || true
    kill $(pgrep -f gdpr_controller) 2>/dev/null || true
    kill $(pgrep -f rocksdb_server) 2>/dev/null || true
    kill $(pgrep -f redis-server) 2>/dev/null || true
    
    echo "Waiting for ports to become inactive"
    wait_for_tcp_shutdown "$controller_address" "$controller_port"
    wait_for_tcp_shutdown "${db_address#tcp://}" "$db_port"
    
    # Remove unix sockets
    rm -f "${PATHS[REDIS_SOCKET]}" "${PATHS[ROCKSDB_SOCKET]}"
    
    echo "Cleaning up files"
    rm -f "${CONFIG[TMP_DIR]}"/server.txt "${CONFIG[TMP_DIR]}"/controller.txt "${CONFIG[TMP_DIR]}"/clients.txt
    rm -rf "${CONFIG[DB_DUMP_DIR]}"
    
    sleep 3
    if pgrep -f qemu > /dev/null; then
        echo "Forcefully terminating remaining QEMU processes..."
        sudo kill -SIGKILL $(pgrep -f qemu) 2>/dev/null || true
    fi
    
    echo "All cleanup operations completed."
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
    shift 7
    
    prepare_experiment "$results_csv_file"
    
    local db_address_formatted="${db_address}:${db_port}"
    if [[ $db == "redis" ]]; then
        db_address_formatted="tcp://${db_address_formatted}"
    fi
    
    case "$experiment_type" in
        "native_direct")
            run_server "$db" "native" "$db_address" "$db_port" "${CONFIG[DB_DUMP_DIR]}" "${CONFIG[TMP_DIR]}/server.txt"
            run_client "direct" "$workload" "$n_clients" "$db_address_formatted" "" "${CONFIG[TMP_DIR]}/clients.txt" "$db"
            collect_results "$workload" "direct" "$db" "$n_clients" "$results_csv_file"
            ;;
        "native_ctl")
            local controller="$1"
            local controller_address="$2"
            local controller_port="$3"
            local config="$4"
            
            run_server "$db" "native" "$db_address" "$db_port" "${CONFIG[DB_DUMP_DIR]}" "${CONFIG[TMP_DIR]}/server.txt"
            run_controller "$controller" "native" "$controller_address" "$controller_port" "$db" "$db_address_formatted" "${CONFIG[DB_DUMP_DIR]}" "${CONFIG[TMP_DIR]}/controller.txt" ""
            run_client "controller" "$workload" "$n_clients" "$controller_address" "$controller_port" "${CONFIG[TMP_DIR]}/clients.txt" "$config"
            collect_results "$workload" "$controller" "$db" "$n_clients" "$results_csv_file"
            ;;
        "CVM_direct")
            run_server "$db" "CVM" "$db_address" "$db_port" "${CONFIG[DB_DUMP_DIR]}" "${CONFIG[TMP_DIR]}/server.txt"
            run_client "direct" "$workload" "$n_clients" "$db_address_formatted" "" "${CONFIG[TMP_DIR]}/clients.txt" "$db"
            collect_results "$workload" "direct" "$db" "$n_clients" "$results_csv_file"
            ;;
        "CVM_passthrough"|"CVM_gdpr")
            local controller_type="${experiment_type#CVM_}"
            local controller_address="$1"
            local controller_port="$2"
            local config="$3"
            
            run_controller "$controller_type" "CVM" "$controller_address" "$controller_port" "$db" "$db_address" "${CONFIG[DB_DUMP_DIR]}" "${CONFIG[TMP_DIR]}/controller.txt" "${CONFIG[TMP_DIR]}/server.txt"
            run_client "controller" "$workload" "$n_clients" "$controller_address" "$controller_port" "${CONFIG[TMP_DIR]}/clients.txt" "$config"
            collect_results "$workload" "$controller_type" "$db" "$n_clients" "$results_csv_file"
            ;;
    esac
    
    cleanup "$controller_address" "$controller_port" "$db" "$db_address" "$db_port"
}

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
