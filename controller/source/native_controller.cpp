#include <iostream>
#include <string>

#include "redis.hpp"
#include "query.hpp"
// #include "argh.hpp"

auto main() -> int
{ 
  /* initialize the client object that exports put/get/delete API */
  redis_client client("tcp://127.0.0.1:6379");
  
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
      std::cout << "Exiting..." << std::endl;
      break;
    } else {
      std::cout << "Invalid command" << std::endl;
      break;
    }
  }

  return 0;
}
