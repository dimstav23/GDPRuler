#include <iostream>
#include <string>
#include "absl/strings/match.h" // for StartsWith function
#include <chrono>

#include "default_policy.hpp"
#include "query.hpp"
#include "query_rewriter.hpp"
#include "gdpr_filter.hpp"
#include "common.hpp"
#include "kv_client/factory.hpp"
// #include "argh.hpp"

using controller::default_policy;
using controller::query;
using controller::query_rewriter;
using controller::gdpr_filter;

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
    query query_args(input_query);

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
        // client->get(query_args.key());
        auto res = client->get(query_args.key());
        gdpr_filter filter(res);
        if (filter.validate()) {
          // TODO: write the value to the client socket
        } 
        else {
          // TODO: write NULL value to the client socket
        }
      }
      else if (query_args.cmd() == "put") {
        // auto res = client->get(query_args.key());
        // gdpr_filter filter(res);
        // filter.validate();
        query_rewriter rewriter(query_args, def_policy, query_args.value());
        auto res = client->put(query_args.key(), rewriter.new_value());
        // TODO: write res value to the client socket
      }
      else if (query_args.cmd() == "del") {
        // auto res = client->get(query_args.key());
        // gdpr_filter filter(res);
        // filter.validate();
        auto res = client->del(query_args.key());
        // TODO: write res value to the client socket
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
