#pragma once

#include <string>
#include <unordered_map>
#include <shared_mutex>
#include <optional>
#include <list>
#include <algorithm>
#include <memory>
#include <atomic>

#include "gdpr_metadata.hpp"

namespace controller {

/**
 * @brief Template class for a thread-safe LRU cache for GDPR metadata
 * 
 * This class implements a fixed-size cache with O(1) lookup, insertion, and deletion
 * operations. It uses a reader/writer lock for thread safety with multiple readers
 * and single writer.
 * 
 * @tparam T The type of metadata to be cached
 */
template <typename T>
class GlobalGdprMetadataCache {
public:
  struct string_hash {
    using is_transparent = void;
    size_t operator()(const std::string& str) const { return std::hash<std::string>{}(str); }
    size_t operator()(std::string_view sv) const { return std::hash<std::string_view>{}(sv); }
  };
  
  /**
   * @brief Constructs a new GlobalGdprMetadataCache with specified capacity
   * 
   * @param capacity Maximum number of items to store in the cache
   */
  #ifdef CACHE_STATS
  explicit GlobalGdprMetadataCache(size_t capacity) 
      : m_capacity(capacity), 
        m_curr_size(0),
        m_hits(0),
        m_misses(0) {}
  #else
  explicit GlobalGdprMetadataCache(size_t capacity) 
      : m_capacity(capacity), 
        m_curr_size(0) {}
  #endif

  /**
   * @brief Get metadata for a given key if it exists in the cache
   * 
   * @param key The key to lookup
   * @return std::optional<T> The metadata if found, std::nullopt otherwise
   */
  auto cache_get(std::string_view key) -> std::optional<T> {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    
    auto it = m_cache_map.find(key);
    if (it != m_cache_map.end()) {
      // Move the accessed item to the front of the list (most recently used)
      // m_cache_list.splice(m_cache_list.begin(), m_cache_list, it->second.list_it);
      #ifdef CACHE_STATS
      m_hits.fetch_add(1, std::memory_order_relaxed);
      #endif
      return it->second.gdpr_metadata;
    }
    
    #ifdef CACHE_STATS
    m_misses.fetch_add(1, std::memory_order_relaxed);
    #endif

    return std::nullopt;
  }

  /**
   * @brief Insert or update metadata for a given key
   * 
   * @param key The key to insert or update
   * @param gdpr_metadata The metadata to store
   */
  auto cache_put(std::string_view key, T gdpr_metadata) -> void {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    
    auto it = m_cache_map.find(key);
    if (it != m_cache_map.end()) {
      // Key exists, update its gdpr_metadata and move it to the front
      it->second.gdpr_metadata = std::move(gdpr_metadata);
      m_cache_list.splice(m_cache_list.begin(), m_cache_list, it->second.list_it);
      return;
    }
    
    // If cache is at capacity, remove the least recently used item
    if (m_curr_size >= m_capacity) {
      auto last = m_cache_list.back();
      m_cache_map.erase(last);
      m_cache_list.pop_back();
      --m_curr_size;
    }
    
    // Insert new item at the front
    std::string key_str(key);  // Convert to std::string
    m_cache_list.push_front(key_str);
    m_cache_map[key_str] = {std::move(gdpr_metadata), m_cache_list.begin()};
    ++m_curr_size;
  }

  /**
   * @brief Remove an item from the cache
   * 
   * @param key The key to remove
   * @return true if the key was found and removed, false otherwise
   */
  auto cache_remove(std::string_view key) -> bool {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    
    auto it = m_cache_map.find(key);
    if (it != m_cache_map.end()) {
      m_cache_list.erase(it->second.list_it);
      m_cache_map.erase(it);
      --m_curr_size;
      return true;
    }
    
    return false;
  }

  /**
   * @brief Clear all items from the cache
   */
  auto cache_clear() -> void {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_cache_map.clear();
    m_cache_list.clear();
    m_curr_size = 0;
  }

  /**
   * @brief Get the current size of the cache
   * 
   * @return size_t The number of items in the cache
   */
  [[nodiscard]] auto cache_curr_size() const -> size_t {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_curr_size;
  }

  /**
   * @brief Get the capacity of the cache
   * 
   * @return size_t The maximum number of items the cache can hold
   */
  [[nodiscard]] auto cache_capacity() const -> size_t {
    return m_capacity;
  }

  #ifdef CACHE_STATS
  /**
   * @brief Get the hit rate of the cache
   * 
   * @return double The percentage of successful lookups
   */
  [[nodiscard]] auto cache_hit_rate() const -> double {
    uint64_t hits = m_hits.load(std::memory_order_relaxed);
    uint64_t misses = m_misses.load(std::memory_order_relaxed);
    uint64_t total = hits + misses;
    return total > 0 ? static_cast<double>(hits) / total : 0.0;
  }

  /**
   * @brief Get the number of cache hits
   * 
   * @return uint64_t The number of cache hits
   */
  [[nodiscard]] auto cache_hits() const -> uint64_t {
    return m_hits.load(std::memory_order_relaxed);
  }

  /**
   * @brief Get the number of cache misses
   * 
   * @return uint64_t The number of cache misses
   */
  [[nodiscard]] auto cache_misses() const -> uint64_t {
    return m_misses.load(std::memory_order_relaxed);
  }
  #endif

  /**
   * @brief Singleton instance getter
   * 
   * @param capacity The capacity for the cache (only used on first call)
   * @return A reference to the singleton cache instance
   */
  static auto get_instance(size_t capacity = 1024) -> GlobalGdprMetadataCache<T>& {
    static GlobalGdprMetadataCache<T> cache_instance(capacity);
    return cache_instance;
  }

private:
  // Cache entry structure
  struct CacheEntry {
    T gdpr_metadata;
    typename std::list<std::string>::iterator list_it;
  };

  // Maximum capacity of the cache
  size_t m_capacity;
  
  // Current size of the cache
  size_t m_curr_size;
  
  #ifdef CACHE_STATS
  // Cache hit/miss counters for performance tracking
  mutable std::atomic<uint64_t> m_hits;
  mutable std::atomic<uint64_t> m_misses;
  #endif
  
  // LRU list to track item usage order (most recent at front)
  mutable std::list<std::string> m_cache_list;
  
  // Map for O(1) lookups
  std::unordered_map<std::string, CacheEntry, string_hash, std::equal_to<>> m_cache_map;
  
  // Reader/writer lock for thread safety
  mutable std::shared_mutex m_mutex;
};

/**
 * @brief Typedef for the GDPR metadata cache
 * 
 * This defines the specific type of metadata cache used in the application.
 * Can be modified to accommodate different metadata types in the future.
 */
using GdprMetadataCache = GlobalGdprMetadataCache<std::string>;

} // namespace controller