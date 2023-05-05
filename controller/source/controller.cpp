#include <iostream>
#include <string>
#include "absl/strings/match.h" // for StartsWith function
#include <chrono>
#include <cassert>

#include "default_policy.hpp"
#include "query.hpp"
#include "query_rewriter.hpp"
#include "gdpr_filter.hpp"
#include "common.hpp"
#include "kv_client/factory.hpp"
#include "logging/logger.hpp"
#include "logging/monitor.hpp"
#include "gdpr_regulator.hpp"

using controller::default_policy;
using controller::query;
using controller::query_rewriter;
using controller::gdpr_filter;
using controller::logger;
using controller::gdpr_monitor;
using controller::gdpr_regulator;

auto handle_get(const std::unique_ptr<kv_client> &client, 
                const query &query_args,
                const default_policy &def_policy) -> void 
{
  auto res = client->get(query_args.key());
  auto filter = std::make_shared<gdpr_filter>(res);

  // Check if the retrieved value requires logging
  auto monitor = gdpr_monitor(filter, query_args, def_policy);

  bool is_valid = filter->validate(query_args, def_policy);
  // Perform the logging of the (in)valid operation -- if needed
  monitor.monitor_query(is_valid);

  if (is_valid) {
    // if the key exists and complies with the gdpr rules
    // then return the value of the get operation
    // TODO: write the value to the client socket
    assert(res);
  }
  else {
    // TODO: write FAILED_GET value to the client socket
  }
}

auto handle_put(const std::unique_ptr<kv_client> &client, 
                const query &query_args,
                const default_policy &def_policy) -> void 
{
  auto res = client->get(query_args.key());
  auto filter = std::make_shared<gdpr_filter>(res);

  bool is_valid = true;
  // if the key does not exist, perform the put
  if (!res) {
    // If no value is returned, check the respective query args
    // If no query args are specified, enforce the default policy for monitoring
    auto monitor = gdpr_monitor(query_args, def_policy);
    // construct the gdpr metadata for the new value
    query_rewriter rewriter(query_args, def_policy, query_args.value());
    // Perform the logging of the valid operation -- if needed
    monitor.monitor_query(is_valid, rewriter.new_value());
    auto ret_val = client->put(query_args.key(), rewriter.new_value());

    // TODO: write ret_val value to the client socket
    assert(ret_val);
  }
  // if the key exists and complies with the gdpr rules, perform the put
  else if ((is_valid = filter->validate(query_args, def_policy))) {
    // Check if the retrieved value requires logging
    // the query args do not need to be checked since they cannot update the 
    // gpdr metadata of the value -- only putm operations can
    auto monitor = gdpr_monitor(filter, query_args, def_policy);
    // update the current value with the new one without modifying any metadata
    query_rewriter rewriter(res.value(), query_args.value());
    // Perform the logging of the valid operation -- if needed
    monitor.monitor_query(is_valid, rewriter.new_value());
    auto ret_val = client->put(query_args.key(), rewriter.new_value());

    // TODO: write ret_val value to the client socket
    assert(ret_val);
  }
  else {
    // Perform the logging of the invalid operation -- if needed
    auto monitor = gdpr_monitor(filter, query_args, def_policy);
    monitor.monitor_query(is_valid);
    // TODO: write FAILED_PUT value to the client socket
  }
}

auto handle_delete(const std::unique_ptr<kv_client> &client, 
                  const query &query_args,
                  const default_policy &def_policy) -> void 
{
  auto res = client->get(query_args.key());
  auto filter = std::make_shared<gdpr_filter>(res);

  // Check if the retrieved value requires logging
  auto monitor = gdpr_monitor(filter, query_args, def_policy);

  bool is_valid = filter->validate(query_args, def_policy);
  // Perform the logging of the (in)valid operation -- if needed
  monitor.monitor_query(is_valid);

  if (is_valid) {
    // if the key exists and complies with the gdpr rules
    // then perform the delete operation
    auto ret_val = client->del(query_args.key());
    // TODO: write ret_val value to the client socket
    assert(ret_val);
  }
  else {
    // TODO: write FAILED_DELETE value to the client socket
  }
}

auto handle_get_logs(const std::unique_ptr<kv_client> &client, 
                  const query &query_args,
                  const default_policy &def_policy) -> void 
{

  auto regulator = gdpr_regulator();
  
  if (query_args.log_key() == "read_all") {
    std::cout << "Reading all the log files..." << std::endl;
    std::vector<std::string> log_files = regulator.retrieve_logs();
    // TODO: redirect this output to the regulator secure channel
    for (const auto& log : log_files) {
      std::cout << "Log file: " << log << std::endl;
      std::vector<std::string> log_entries = regulator.read_log(log);
      for (const auto& entry : log_entries) {
        std::cout << entry << std::endl;
      }
    }
  }
  else if (query_args.log_key() == "dir") {
    std::cout << "Available log files:" << std::endl;
    std::vector<std::string> log_files = regulator.retrieve_logs();
    // TODO: redirect this output to the regulator secure channel
    for (const auto& log : log_files) {
      std::cout << log << std::endl;
    }
  }
  else {
    std::cout << "Reading the log file of key " << query_args.log_key() << ":" << std::endl;
    std::vector<std::string> log_entries = regulator.read_key_log(query_args.log_key());
    // TODO: redirect this output to the regulator secure channel
    for (const auto& entry : log_entries) {
      std::cout << entry << std::endl;
    }
  }
}

auto main(int argc, char* argv[]) -> int
{ 
  // read the default policy line
  std::string def_policy_line;
  std::getline(std::cin, def_policy_line);
  default_policy def_policy;

  if (absl::StartsWith(def_policy_line, controller::def_policy_prefix)) {
    def_policy = default_policy{def_policy_line};
  } else {
    std::cout << "Invalid default policy provided\n";
    return 1;
  }

  /* initialize the client object that exports put/get/delete API */
  auto args = std::span(argv, static_cast<size_t>(argc));
  std::string db_type = get_command_line_argument(args, "--db");
  if (db_type.empty()) {
    std::cerr << "--db {redis,rocksdb} argument is not passed!" << std::endl;
    std::quick_exit(1);
  }
  std::string db_address = get_command_line_argument(args, "--address");
  std::unique_ptr<kv_client> client = kv_factory::create(db_type, db_address);
  
  // set the log path based on the input parameter
  const std::string log_path = get_command_line_argument(args, "--logpath");
  logger::get_instance()->init_log_path(log_path);

  auto start = std::chrono::high_resolution_clock::now();

  /* read the upcoming queries -- one per line */
  std::string input_query;
  while (true) {
    std::getline(std::cin, input_query);
    const query query_args(input_query);

    if (query_args.cmd() == "exit") [[unlikely]] {
      // std::cout << "Exiting..." << std::endl;
      break;
    }
    else if (query_args.cmd() == "invalid") [[unlikely]] {
      std::cout << "Invalid command" << std::endl;
      break;
    }
    else [[likely]] {
      if (query_args.cmd() == "get") {
        handle_get(client, query_args, def_policy);
      }
      else if (query_args.cmd() == "put") {
        handle_put(client, query_args, def_policy);
      }
      else if (query_args.cmd() == "del") {
        handle_delete(client, query_args, def_policy);
      }
      else if (query_args.cmd() == "putm") { /* ignore for now */
        continue;
      }
      else if (query_args.cmd() == "getm") { /* ignore for now */
        continue;
      }
      else if (query_args.cmd() == "delm") { /* ignore for now */
        continue;
      }
      else if (query_args.cmd() == "getLogs") {
        // client resembles the regulator
        handle_get_logs(client, query_args, def_policy);
      }
      else {
        std::cout << "Invalid command: " << query_args.cmd() << std::endl;
        break;
      }
    }
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto duration = end - start;
  auto duration_in_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
  auto duration_in_s = static_cast<long double>(duration_in_ns) / s2ns;
  std::cout.precision(ns_precision);
  std::cout << "Controller time: "
            << std::fixed << duration_in_s
            << " s" << std::endl;

  return 0;
}
