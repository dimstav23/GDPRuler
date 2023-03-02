# rocksdb_server

This directory contains the source code of a TCP based rocksdb server. 

## How to run server

Program binary can be built alongside the gdpr_controller CMake. The rocksdb server is a standalone CLI program which can be executed with "./rocksdb_server <listening_port> <rocksdb_storage_path>" command. Note that it expects exactly two arguments in this order.

Example execution: **./rocksdb_server 15001 ./db**

## File definitions

* message.hpp file contains the expected request and response message protocols.
* rocksdb_proxy.hpp file contains an interface to interact with the actual rocksdb library.
* server.cpp file contains the entry point to the program. Using boost::asio library, it listens to the connections and serve them.

## How to test server

1. Using netcat on terminal: 
    1. Start rocksdb_server using above command.
    2. Execute: **nc localhost 15001** command to have an interactive tcp connection
    3. Type queries and see responses. (Example: "put 5 15", "get 5", "del 5", ...)
2. Using Python scripts and workloads:
    1. Start rocksdb_server using above command.
    2. Execute: **python GDPRuler.py --config ./configs/owner_policy.json --workload ./workload_traces/workloadf_test --db rocksdb --address 127.0.0.1:15001**
