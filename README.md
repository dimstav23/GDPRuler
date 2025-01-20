# GDPRuler

### Repository structure
[sev_demo](./sev_demo): Folder containing documentation for setting up an ubuntu based AMD SEV VM and perform proof tests.

[policy_compiler](./policy_compiler): Folder containing the policy compiler.

[configs](./configs): Folder containing sample configs for data owner, data controller, data processor (3rd party) and regulator.

[controller](./controller): Folder containing the core code of the data controller.

[KVs](./KVs): Folder containing the KVs submodules.

[sev-tool](./sev-tool/): submodule providing AMD SEV functionalities

[ycsb_trace_generator](./ycsb_trace_generator/): submodule containing a modified version of GDPRBench (YCSB-based) to produce workload traces

## Build instructions

### 0. Dev environment
To enter the development environment with all the required dependencies, use:
```
$ nix develop
``` 

### 1. Make sure you have fetched all the submodules:
```
$ git submodule update --init --recursive
```

### 2. Build the `GDPR controller`:
```
$ cd controller
$ cmake -S . -B build -D CMAKE_BUILD_TYPE=Release
$ cmake --build build
```

**Useful options:**
- Enable/disable the encryption with `-D ENCRYPTION_ENABLED=ON/OFF` (defaults to `ON`)
- Enable/disable AddressSanitizer with `-D ASAN_ENABLED=ON/OFF` (defaults to `OFF`)
- Enable/disable ThreadSanitizer with `-D TSAN_ENABLED=ON/OFF` (defaults to `OFF`)

### 3. Compile `redis` (to build the `redis-server` binary):
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
$ python3 native_ctl.py --db [redis/rocksdb]
```
For the native GDPR controller:
```
$ python3 GDPRuler.py --config [user_config] --db [redis/rocksdb]
```

For more command line options, please consult [`native_ctl.py`](./native_ctl.py) and [`GDPRuler.py`](./GDPRuler.py).

### 4. Run the client(s) with a desired workload:
```
$ python3 client.py --workload [workload_trace_file] --clients [num_of_clients]
```

For more command line options, please consult [`client.py`](./client.py).

## VM Setup instructions
For instructions on how to set up the client and server SEV VMs, 
please consult the respective [README](./AMD_SEV_SNP/README.md).

## VM sample execution

**TODO** @dimstav23