#include <iostream>
#include <string>
#include "absl/strings/match.h" // for StartsWith function
#include <chrono>

#include "default_policy.hpp"
#include "query.hpp"
#include "query_rewriter.hpp"
#include "common.hpp"
#include "redis.hpp"
// #include "argh.hpp"

using controller::default_policy;
using controller::query;
using controller::query_rewriter;

auto main() -> int
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
  redis_client client("tcp://127.0.0.1:6379");

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
      // query_args.print();
      if (query_args.cmd() == "get") {
        client.get(query_args.key());
      }
      else if (query_args.cmd() == "put") {
        query_rewriter rewriter(query_args, def_policy, query_args.value());
        client.put(query_args.key(), rewriter.new_value());
      }
      else if (query_args.cmd() == "del") {
        client.del(query_args.key());
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
