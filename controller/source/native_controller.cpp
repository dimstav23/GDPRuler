#include <iostream>
#include <string>
#include <chrono>

#include "kv_client/factory.hpp"
#include "query.hpp"
#include "common.hpp"
// #include "argh.hpp"

auto main(int argc, char* argv[]) -> int
{ 
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

  std::string command;
  std::string key;
  std::string value;
  while (true) {
    std::cin >> command;
    if (command == "get") {
      std::cin >> key;
      client->get(key);
    } else if (command == "put") {
      std::cin >> key;
      std::cin >> value;
      client->put(key, controller::get_value());
    } else if (command == "del") {
      std::cin >> key;
      client->del(key);
    } else if (command == "exit") {
      // std::cout << "Exiting..." << std::endl;
      break;
    } else {
      std::cout << "Invalid command" << std::endl;
      break;
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
