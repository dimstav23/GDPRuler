#include <iostream>
#include <string>
#include <chrono>

#include "redis.hpp"
#include "query.hpp"
#include "common.hpp"
// #include "argh.hpp"

auto main() -> int
{ 
  /* initialize the client object that exports put/get/delete API */
  redis_client client("tcp://127.0.0.1:6379");
  
  auto start = std::chrono::high_resolution_clock::now();

  std::string command;
  std::string key;
  std::string value;
  while (true) {
    std::cin >> command;
    if (command == "get") {
      std::cin >> key;
      client.get(key);
    } else if (command == "put") {
      std::cin >> key;
      std::cin >> value;
      client.put(key, controller::get_value());
    } else if (command == "del") {
      std::cin >> key;
      client.del(key);
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
