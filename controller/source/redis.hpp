#include <iostream>
#include <string>

#include <sw/redis++/redis++.h>

class redis_client
{
  sw::redis::Redis m_redis;

public:
  explicit redis_client(const std::string& addr)
      : m_redis(addr)
  {
  }

  auto get(const std::string& key) -> std::optional<std::string>
  {
    auto result = m_redis.get(key);
    if (result) {
      // Key exists. Dereference val to get the string result.
      // std::cout << "GET operation done with key: " << key
      //           << " and value: " << *result << std::endl;
    } else {
      // Redis server returns a NULL Bulk String Reply.
      // It's invalid to dereference a null Optional<T> object.
      std::cout << "GET operation failed" << std::endl;
    }
    return result;
  }

  auto put(const std::string& key, const std::string& value) -> bool
  {
    bool res = true;
    auto result = m_redis.set(key, value);
    if (result) {
      // std::cout << "PUT operation done with key: " << key
      //           << " and value: " << value << std::endl;
    } else {
      std::cout << "PUT operation failed" << std::endl;
      res = false;
    }
    return res;
  }

  auto del(const std::string& key) -> bool
  {
    bool res = true;
    auto result = m_redis.del(key);
    if (result == 1) {
      // std::cout << "DEL operation done with key: " << key << std::endl;
    } else if (result == 0) {
      std::cout << "DEL operation failed -- key " << key << " not found"
                << std::endl;
      res = false;
    }
    return res;
  }
};