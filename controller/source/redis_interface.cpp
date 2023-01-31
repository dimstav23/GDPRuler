#include <iostream>
#include <string>
#include <sw/redis++/redis++.h>

using sw::redis::Redis;

class redis_client
{
  Redis m_redis;

public:
  redis_client() : m_redis("tcp://127.0.0.1:6379") { }

  auto get(const std::string& key) -> std::optional<std::string>
  {
    auto result = m_redis.get(key);
    if (result) {
      // Key exists. Dereference val to get the string result.
      std::cout << "GET operation done with key: " << key
                << " and value: " << *result << std::endl;
    } else {
      // Redis server returns a NULL Bulk String Reply.
      // It's invalid to dereference a null Optional<T> object.
      std::cout << "GET operation failed" << std::endl;
    }
    return result;
  }

  auto set(const std::string& key, const std::string& value) -> bool
  {
    bool res = true;
    auto result = m_redis.set(key, value);
    if (result) {
      std::cout << "SET operation done with key: " << key
                << " and value: " << value << std::endl;
    } else {
      std::cout << "SET operation failed" << std::endl;
      res = false;
    }
    return res;
  }

  auto del(const std::string& key) -> bool
  {
    bool res = true;
    auto result = m_redis.del(key);
    if (result == 1) {
      std::cout << "DEL operation done with key: " << key << std::endl;
    } else if (result == 0) {
      std::cout << "DEL operation failed -- key " << key << " not found" << std::endl;
      res = false;
    }
    return res;
  }
};

auto main() -> int
{
  redis_client client;
  std::string command;
  std::string key;
  std::string value;

  /* for now we assume inputs in the form:
   * get (key)
   * put (key) (value)
   * del (key)
   * exit
   */

  while (true) {
    std::cin >> command;
    if (command == "get") {
      std::cin >> key;
      client.get(key);
    } else if (command == "set") {
      std::cin >> key;
      std::cin >> value;
      client.set(key, value);
    } else if (command == "del") {
      std::cin >> key;
      client.del(key);
    } else if (command == "exit") {
      std::cout << "Exiting..." << std::endl;
      break;
    } else {
      std::cout << "Invalid command" << std::endl;
    }
  }
  return 0;
}
