#include <iostream>
#include <string>
#include "absl/strings/match.h" // for StartsWith function

#include "default_policy.hpp"
#include "query.hpp"
#include "redis.hpp"
// #include "argh.hpp"

using controller::default_policy;
using controller::query;

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
  // /* output expected from GDPRuler.py */
  // std::cout << "default_policy:OK" << "\n";

  /* initialize the client object that exports put/get/delete API */
  redis_client client("tcp://127.0.0.1:6379");

  /* read the upcoming queries -- one per line */
  std::string input_query;
  while (true) {
    std::getline(std::cin, input_query);
    query query_args(input_query);

    if (query_args.cmd() == "exit") [[unlikely]] {
      std::cout << "Exiting..." << std::endl;
      break;
    }
    else if (query_args.cmd() == "invalid") [[unlikely]] {
      std::cout << "Invalid command" << std::endl;
      break;
    }
    else [[likely]] {
      // query_args.rewrite();
      continue;
    }
  }
  
  // std::string key;
  // std::string value;
  
  // while (true) {
  //   std::cin >> command;
  //   if (command == "get") {
  //     std::cin >> key;
  //     // client.get(key);
  //   } else if (command == "put") {
  //     std::cin >> key;
  //     std::cin >> value;
  //     // client.put(key, value);
  //   } else if (command == "del") {
  //     std::cin >> key;
  //     // client.del(key);
  //   } else if (command == "exit") {
  //     std::cout << "Exiting..." << std::endl;
  //     break;
  //   } else {
  //     std::cout << "Invalid command" << std::endl;
  //     break;
  //   }
  // }
  return 0;
}
