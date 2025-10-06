#include <iostream>
#include <string>

#include <sw/redis++/redis++.h>
#include "kv_client.hpp"

class redis_client : public kv_client
{
private:
  sw::redis::Redis m_redis;
  static constexpr size_t PIPELINE_BATCH_SIZE = 100;

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
        values.reserve(keys.size()); // Pre-allocate
        m_redis.mget(keys.begin(), keys.end(), std::back_inserter(values));
        // Move values directly into result set
        result_values.reserve(result_values.size() + values.size());
        for (auto&& val : values) {
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

  auto get_prefix_kv_pairs(std::string_view key_prefix) -> std::vector<std::pair<std::string, std::string>> override
  {
    auto cursor = 0LL;
    std::string pattern = std::string(key_prefix) + "*";
    std::vector<std::pair<std::string, std::string>> result_pairs;

    while (true) {
      std::unordered_set<std::string> keys;
      cursor = m_redis.scan(cursor, pattern, std::inserter(keys, keys.begin()));
      
      if (!keys.empty()) {
        std::vector<std::optional<std::string>> values;
        values.reserve(keys.size());
        m_redis.mget(keys.begin(), keys.end(), std::back_inserter(values));
        
        // Pair up keys with their values
        auto key_it = keys.begin();
        for (auto&& val : values) {
          if (val) {
            result_pairs.emplace_back(*key_it, std::move(*val));
          }
          ++key_it;
        }
      }
      
      if (cursor == 0) {
        break;
      }
    }

    return result_pairs;
  }
  
  auto putm(std::vector<std::pair<std::string, std::string>>& key_value_pairs) -> std::vector<bool> override 
  {
    if (key_value_pairs.empty()) return {};
        
    std::vector<bool> results;
    results.reserve(key_value_pairs.size());
    
    // Process in configurable-sized batches
    for (size_t i = 0; i < key_value_pairs.size(); i += PIPELINE_BATCH_SIZE) {
      size_t batch_end = std::min(i + PIPELINE_BATCH_SIZE, key_value_pairs.size());
      
      // Process one batch
      auto batch_results = process_putm_pipeline_batch(key_value_pairs, i, batch_end);
      
      // Append batch results to total results
      results.insert(results.end(), 
                      std::make_move_iterator(batch_results.begin()),
                      std::make_move_iterator(batch_results.end()));
    }
    return results;
  }

  auto deletem(std::vector<std::string>& keys) -> std::vector<bool> override 
  {
    if (keys.empty()) return {};
        
    std::vector<bool> results;
    results.reserve(keys.size());
    
    // Process in configurable-sized batches
    for (size_t i = 0; i < keys.size(); i += PIPELINE_BATCH_SIZE) {
      size_t batch_end = std::min(i + PIPELINE_BATCH_SIZE, keys.size());
      
      // Process one batch
      auto batch_results = process_deletem_pipeline_batch(keys, i, batch_end);
      
      // Append batch results to total results
      results.insert(results.end(), 
                      std::make_move_iterator(batch_results.begin()),
                      std::make_move_iterator(batch_results.end()));
    }
    return results;
  }

  auto process_putm_pipeline_batch(std::vector<std::pair<std::string, std::string>>& pairs,
                               size_t start, size_t end) -> std::vector<bool> 
  {
    std::vector<bool> batch_results;
    size_t batch_size = end - start;
    batch_results.reserve(batch_size);
    
    try {
      // Create pipeline on existing connection
      auto pipe = m_redis.pipeline(false);  // Reuses m_redis connection
      
      // Add all operations in this batch
      for (size_t i = start; i < end; ++i) {
        pipe.set(std::move(pairs[i].first), std::move(pairs[i].second));
      }
      
      // Execute pipeline
      auto replies = pipe.exec();
      
      // Check each operation result
      for (size_t i = 0; i < batch_size; ++i) {
        try {
          auto result = replies.get<bool>(i);
          batch_results.push_back(result);
        } catch (const std::exception& e) {
          batch_results.push_back(false);
        }
      }
    } catch (const std::exception& e) {
      // Entire batch failed - e.g., connection issue
      batch_results.resize(batch_size, false);
    }
    
    return batch_results;
  }

  // Process deletes using pipeline
  auto process_deletem_pipeline_batch(std::vector<std::string>& keys,
                               size_t start, size_t end) -> std::vector<bool> 
  {
    std::vector<bool> batch_results;
    size_t batch_size = end - start;
    batch_results.reserve(batch_size);
    
    try {
      // Create pipeline on existing connection
      auto pipe = m_redis.pipeline(false);  // Reuses m_redis connection
      
      // Add all operations in this batch
      for (size_t i = start; i < end; ++i) {
        pipe.del(std::move(keys[i]));
      }
      
      // Execute pipeline
      auto replies = pipe.exec();
      
      // Check each operation result
      for (size_t i = 0; i < batch_size; ++i) {
        try {
          // DEL returns long long (number of keys deleted)
          auto deleted_count = replies.get<long long>(i);
          batch_results.push_back(deleted_count > 0);
        } catch (const std::exception& e) {
          batch_results.push_back(false);
        }
      }
    } catch (const std::exception& e) {
      // Entire batch failed - e.g., connection issue
      batch_results.resize(batch_size, false);
    }
    
    return batch_results;
  }

  // Process deletes using batch of delete operations
  auto process_deletem_batch(std::vector<std::string>& keys,
                               size_t start, size_t end) -> std::vector<bool> 
  {
    std::vector<bool> batch_results;
    size_t batch_size = end - start;
    batch_results.reserve(batch_size);
    
    try {
      // Prepare keys for this batch
      std::vector<std::string> batch_keys(keys.begin() + start, 
                                          keys.begin() + end);
      
      // Single UNLINK with multiple keys
      auto deleted_count = m_redis.unlink(batch_keys.begin(), 
                                        batch_keys.end());
      
      // All succeeded if count matches
      bool all_success = (deleted_count == batch_size);
      batch_results.resize(batch_size, all_success);
      
    } catch (const std::exception& e) {
      batch_results.resize(batch_size, false);
    }
    
    return batch_results;
  }
};