# GDPRuler

### Repository structure
[sev_demo](./sev_demo): Folder containing documentation for setting up an ubuntu based AMD SEV VM and perform proof tests.

[policy_compiler](./policy_compiler): Folder containing the policy compiler.

[configs](./configs): Folder containing sample configs for data owner, data controller, data processor (3rd party) and regulator.

[controller](./controller): Folder containing the core code of the data controller.

[KVs](./KVs): Folder containing the KVs submodules.

[scripts](./scripts): Folder containing python wrapper scripts for all considered bare-metal execution variants.

[sev-tool](./sev-tool/): Submodule providing AMD SEV functionalities

[ycsb_trace_generator](./ycsb_trace_generator/): Submodule containing a modified version of GDPRBench (YCSB-based) to produce workload traces

## Build instructions

### 0. Dev environment
To enter the development environment with all the required dependencies, use:
```
$ nix develop
``` 

For the logging experiments you might want to allocate an entire block device and create an `ext4` filesystem on top. 
By default, the scripts expect a mount point at `/scratch/dimitrios/gdpruler_fs`, but you can override this by setting the `GDPRULER_MOUNT_POINT` and `GDPRULER_NVME_DEVICE` environment variables. 
An example execution to set up storage is:
```
$ export GDPRULER_NVME_DEVICE=/dev/nvme1n1
$ export GDPRULER_MOUNT_POINT=/scratch/$USER/gdpruler_fs
$ sudo mkfs.ext4 $GDPRULER_NVME_DEVICE
$ mkdir -p $GDPRULER_MOUNT_POINT
$ sudo mount $GDPRULER_NVME_DEVICE $GDPRULER_MOUNT_POINT -t ext4
$ sudo chown $USER:$(id -gn $USER) $GDPRULER_MOUNT_POINT
```
If you do not set these environment variables, the scripts will default to `/dev/nvme1n1` and `/scratch/dimitrios/gdpruler_fs`

### 1. Make sure you have fetched all the submodules:
```
$ git submodule update --init --recursive
```

### 2. Build the `Logging subsystem`:
```
$ cd gdpr-logger
$ mkdir build
$ cd build
$ cmake .. -D CMAKE_BUILD_TYPE=Release
$ make -j$(nproc)
```

### 3. Build the `GDPR controller`:
```
$ cd controller
$ cmake -S . -B build -D CMAKE_BUILD_TYPE=Release
$ cmake --build build
```

**Useful options:**
- Enable/disable the encryption with `-D ENCRYPTION_ENABLED=ON/OFF` (defaults to `ON`)
- Enable/disable AddressSanitizer with `-D ASAN_ENABLED=ON/OFF` (defaults to `OFF`)
- Enable/disable ThreadSanitizer with `-D TSAN_ENABLED=ON/OFF` (defaults to `OFF`)

### 4. Compile `redis` (to build the `redis-server` binary):
```
$ cd KVs/redis
$ make BUILD_TLS=yes MALLOC=libc
# Optional command to test the success of the installation
$ make test
```

## Native sample execution

### 1. Create the workload traces (~5-10mins):
```
$ cd ycsb_trace_generator
$ bash workload_generator.sh
```
This will create the trace files for the workloads in the [`workload_traces`](./workload_traces) directory.

### 2. Run the KV server.
- For `redis`:
```
$ cd KVs/redis/src
$ ./redis-server --protected-mode no
```
- For `rocksdb`:
```
$ cd controller/build
$ ./rocksdb_server [port] [db_file_location]
```

### 3. Run the controller.
For the native passthrough controller:
```
$ python3 scripts/passthrough.py --db [redis/rocksdb]
```
For the native GDPR controller:
```
$ python3 scripts/GDPRuler.py --db [redis/rocksdb]
```

For more command line options, please consult [`scripts/passthrough.py`](scripts/passthrough.py) and [`scripts/GDPRuler.py`](scripts/GDPRuler.py).

### 4. Run the client(s) with a desired workload:
```
$ python3 scripts/client.py --workload [workload_trace_name] --clients [num_of_clients] --config [user_config/user_config_directory]
```

For more command line options, please consult [`scripts/client.py`](scripts/client.py).

## Configuration

The scripts are designed to work with default storage paths pointing to `/scratch/dimitrios/`. You can customize these paths using environment variables:

```bash
# Mount point for evaluation storage (used by evaluation/common.sh)
export GDPRULER_MOUNT_POINT=/path/to/your/mount/point

# NVMe device for filesystem operations (used by evaluation/common.sh)
export GDPRULER_NVME_DEVICE=/dev/your/device
```

If these environment variables are not set, the scripts will use the defaults:
- `GDPRULER_MOUNT_POINT`: `/scratch/dimitrios/gdpruler_fs`
- `GDPRULER_NVME_DEVICE`: `/dev/nvme1n1`


## VM Setup instructions
For instructions on how to set up the client and server SEV VMs, 
please consult the respective [README](./CVM_setup/README.md).

## VM sample execution

**TODO** @dimstav23