#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <vector>
#include <algorithm>
#include <numeric>
#include <unordered_map>

#include "kv_client/factory.hpp"
#include "query.hpp"
#include "common.hpp"

constexpr int value_size = 1024;
// NOLINTNEXTLINE(cert-err58-cpp)
const std::string dummy_value(value_size, 'x');

struct OperationMetrics {
  std::vector<long double> latencies;
  void add_latency(long double latency) { latencies.push_back(latency); }
  long double max_latency() const { return *std::max_element(latencies.begin(), latencies.end()); }
  long double average_latency() const { return std::accumulate(latencies.begin(), latencies.end(), 0.0L) / latencies.size(); }
};

void record_operation_latency(OperationMetrics& metrics, const std::chrono::time_point<std::chrono::high_resolution_clock>& start) {
  auto end = std::chrono::high_resolution_clock::now();
  long double duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
  metrics.add_latency(duration);
}

void print_metrics_in_csv_format(const std::unordered_map<std::string, OperationMetrics>& op_metrics, long double total_time, size_t total_ops) {
  std::cout << "Metric,Value\n";
  std::cout << "Total client time (ms)," << total_time / 1000.0 << "\n";
  std::cout << "Total client throughput (ops/s)," << total_ops / (total_time / 1000000) << "\n";
  
  // Calculate and print the overall average latency
  long double total_latency = 0.0;
  size_t total_operations = 0;
  for (const auto& [op, metrics] : op_metrics) {
    total_latency += metrics.average_latency() * metrics.latencies.size();
    total_operations += metrics.latencies.size();
  }
  if (total_operations > 0) {
    long double overall_average_latency = total_latency / total_operations;
    std::cout << "Average client latency (ms)," << overall_average_latency / 1000.0 << "\n";
  }

  for (const auto& [op, metrics] : op_metrics) {
    std::cout << "Average client latency for " << op << " (ms)," << metrics.average_latency() / 1000.0 << "\n";
    std::cout << "Maximum client latency for " << op << " (ms)," << metrics.max_latency() / 1000.0 << "\n";
  }
}

// code for testing the native KV stores
// the default server address (localhost) & ports are used (6379 for redis, 15001 for rocksdb) 

auto main(int argc, char* argv[]) -> int
{ 
  /* initialize the client object that exports put/get/delete API */
  auto args = std::span(argv, static_cast<size_t>(argc));
  std::string db_type = get_command_line_argument(args, "--db");
  if (db_type.empty()) {
    std::cerr << "--db {gdpr_redis,redis,rocksdb} argument is not passed!" << std::endl;
    std::quick_exit(1);
  }
  std::string db_address = get_command_line_argument(args, "--address");
  std::unique_ptr<kv_client> client = kv_factory::create(db_type, db_address);
  
  std::unordered_map<std::string, OperationMetrics> op_metrics;
  size_t total_operations = 0;

  auto total_start = std::chrono::high_resolution_clock::now();

  std::string command, key, value;
  while (true) {
    std::cin >> command;
    if (command == "get") {
      std::cin >> key;
      auto op_start = std::chrono::high_resolution_clock::now();
      client->gdpr_get(key);
      record_operation_latency(op_metrics[command], op_start);
    } else if (command == "put") {
      std::cin >> key;
      std::cin >> value;
      auto op_start = std::chrono::high_resolution_clock::now();
      client->gdpr_put(key, std::string_view(dummy_value));
      record_operation_latency(op_metrics[command], op_start);
    } else if (command == "del") {
      std::cin >> key;
      auto op_start = std::chrono::high_resolution_clock::now();
      client->gdpr_del(key);
      record_operation_latency(op_metrics[command], op_start);
    // } else if (command == "getm") {
      
    // } else if (command == "putm") {
      
    // } else if (command == "getLogs") {
      
    // }
    } else if (command == "exit") {
      // std::cout << "Exiting..." << std::endl;
      break;
    } else {
      std::cout << "Invalid command" << std::endl;
      break;
    }
    total_operations++;
  }

  auto total_end = std::chrono::high_resolution_clock::now();
  long double total_duration = std::chrono::duration_cast<std::chrono::microseconds>(total_end - total_start).count();

  print_metrics_in_csv_format(op_metrics, total_duration, total_operations);

  return 0;
}