#include <iostream>
#include <string>

#include <sw/redis++/redis++.h>
#include "kv_client.hpp"

class redis_client : public kv_client
{
  sw::redis::Redis m_redis;

public:
  explicit redis_client(const std::string& addr)
      : m_redis(addr)
  {
  }

  auto get(std::string_view key) -> std::optional<std::string> override
  {
    auto result = m_redis.get(key);
    // if (result) {
    //   // Key exists. Dereference val to get the string result.
    //   // #ifndef NDEBUG
    //   // std::cout << "GET operation done with key: " << key
    //   //           << " and value: " << *result << std::endl;
    //   // #endif
    // } else {
    //   // Redis server returns a NULL Bulk String Reply.
    //   // It's invalid to dereference a null Optional<T> object.
    //   // std::cout << "GET operation failed" << std::endl;
    // }
    return result;
  }

  auto put(std::string_view key, std::string_view value) -> bool override
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

  auto del(std::string_view key) -> bool override
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

  auto getm(std::string_view key) -> std::optional<std::string> override
  {
    // auto cursor = 0LL;
    // auto pattern = "*pattern*";
    // auto count = 5;
    // std::unordered_set<std::string> keys;
    // while (true) {
    //   cursor = redis.scan(cursor, pattern, count, std::inserter(keys, keys.begin()));
    //   // Default pattern is "*", and default count is 10
    //   // cursor = redis.scan(cursor, std::inserter(keys, keys.begin()));

    //   if (cursor == 0) {
    //     break;
    //   }
    // }
    auto result = m_redis.get(key);
    return result;
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