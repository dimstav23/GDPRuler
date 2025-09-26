#include <iostream>
#include <string>

#include <sw/redis++/redis++.h>
#include "kv_client.hpp"

class redis_client : public kv_client
{
  sw::redis::Redis m_redis;

public:
  explicit redis_client(const std::string& addr = "unix:///tmp/redis.sock") // use default Unix socket path
      : m_redis(addr)
  {
  }

  inline auto get(std::string_view key) -> std::optional<std::string> override
  {
    auto result = m_redis.get(key);
    // if (result) {
    //   // Key exists. Dereference val to get the string result.
    //   // #ifdef DEBUG
    //   // std::cout << "GET operation done with key: " << key
    //   //           << " and value: " << *result << std::endl;
    //   // #endif
    // } else {
    //   // Redis server returns a NULL Bulk String Reply.
    //   // It's invalid to dereference a null Optional<T> object.
    //   // std::cout << "GET operation failed" << std::endl;
    // }
    return std::move(result);
  }

  inline auto put(std::string_view key, std::string_view value) -> bool override
  {
    bool res = true;
    auto result = m_redis.set(key, value);
    if (result) {
      // std::cout << "PUT operation done with key: " << key
      //           << " and value: " << value << std::endl;
    } else {
      // std::cout << "PUT operation failed" << std::endl;
      res = false;
    }
    return res;
  }

  inline auto del(std::string_view key) -> bool override
  {
    bool res = true;
    auto result = m_redis.del(key);
    if (result == 1) {
      // std::cout << "DEL operation done with key: " << key << std::endl;
    } else if (result == 0) {
      // std::cout << "DEL operation failed -- key " << key << " not found"
      //           << std::endl;
      res = false;
    }
    return res;
  }

  auto getm(std::string_view key_prefix) -> std::vector<std::string> override
  {
    auto cursor = 0LL;
    // add "*" for redis pattern matching -- still key_prefix is the actual prefix
    auto pattern = std::string(key_prefix) + "*"; 
    std::vector<std::string> result_values;

    while (true) {
      std::unordered_set<std::string> keys;
      cursor = m_redis.scan(cursor, pattern, std::inserter(keys, keys.begin()));
      
      if (!keys.empty()) {
        std::vector<std::optional<std::string>> values;
        m_redis.mget(keys.begin(), keys.end(), std::back_inserter(values));
        // Move values directly into result set
        result_values.reserve(result_values.size() + values.size());
        for (auto& val : values) {
          if (val) {
            result_values.emplace_back(std::move(*val)); // Move the value
          }
        }
      }
      
      if (cursor == 0) {
        break;
      }
    }

    return result_values;
  }

  auto putm(std::string_view key, std::string_view value) -> bool override
  {
    bool res = true;
    auto result = m_redis.set(key, value);
    if (result) {
      // std::cout << "PUTM operation done with key: " << key
      //           << " and value: " << value << std::endl;
    } else {
      // std::cout << "PUTM operation failed" << std::endl;
      res = false;
    }
    return res;
  }

};