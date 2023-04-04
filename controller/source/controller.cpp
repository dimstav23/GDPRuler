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

using controller::default_policy;
using controller::query;
using controller::query_rewriter;
using controller::logger;
using controller::gdpr_filter;

// NOLINTNEXTLINE
auto *logger = logger::get_instance();


auto handle_get(const std::unique_ptr<kv_client> &client, 
                const query &query_args,
                const default_policy &def_policy) -> void 
{
  auto res = client->get(query_args.key());
  gdpr_filter filter(res);
  // if the key exists and complies with the gdpr rules
  // then return the value of the get operation
  if (filter.validate(query_args, def_policy)) {
    logger->log(query_args, res.has_value());
    // TODO: write the value to the client socket
    assert(res);
  } 
  else {
    logger->log(query_args, false, "Filter validation failed.");
    // TODO: write FAILED_GET value to the client socket
  }
}

auto handle_put(const std::unique_ptr<kv_client> &client, 
                const query &query_args,
                const default_policy &def_policy) -> void 
{
  auto res = client->get(query_args.key());
  gdpr_filter filter(res);
  // if the key does not exist or it exists and it complies with the gdpr rules
  // then perform the put operation
  if (!res || filter.validate(query_args, def_policy)){ 
    query_rewriter rewriter(query_args, def_policy, query_args.value());
    auto ret_val = client->put(query_args.key(), rewriter.new_value());
    logger->log(query_args, ret_val, "new value: " + rewriter.new_value());
    // TODO: write ret_val value to the client socket
    assert(ret_val);
  }
  else {
    logger->log(query_args, false, "Filter validation failed.");
    // TODO: write FAILED_PUT value to the client socket
  }
}

auto handle_delete(const std::unique_ptr<kv_client> &client, 
                  const query &query_args,
                  const default_policy &def_policy) -> void 
{
  auto res = client->get(query_args.key());
  gdpr_filter filter(res);
  // if the key exists and complies with the gdpr rules
  // then perform the delete operation
  if (filter.validate(query_args, def_policy)) {
    auto ret_val = client->del(query_args.key());
    logger->log(query_args, ret_val);
    // TODO: write ret_val value to the client socket
    assert(ret_val);
  }
  else {
    logger->log(query_args, false, "Filter validation failed.");
    // TODO: write FAILED_DELETE value to the client socket
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
      else if (query_args.cmd() == "getLogs") { /* ignore for now */
        continue;
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
