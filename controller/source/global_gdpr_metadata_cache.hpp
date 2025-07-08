#pragma once

#include <string>
#include <unordered_map>
#include <shared_mutex>
#include <atomic>
#include <memory>
#include <array>
#include <string_view>
#include <random>

namespace controller {

template <typename T>
class ShardedGdprMetadataCache {
private:
    static constexpr size_t NUM_SHARDS = 64;  // Power of 2 for efficient mod
    
    struct CacheEntry {
      std::shared_ptr<const T> data;
    };
    
    struct Shard {
      std::unordered_map<std::string, CacheEntry> entries;
      std::shared_mutex mutex;  // Fine-grained per-shard locking
      std::atomic<size_t> size{0};
      mutable std::mt19937 rng{std::random_device{}()};  // For random eviction
    };
    
    std::array<Shard, NUM_SHARDS> shards;
    const size_t max_size_per_shard;
    
    size_t get_shard_index(std::string_view key) const noexcept {
      return std::hash<std::string_view>{}(key) % NUM_SHARDS;
    }

    // Cache hit/miss counters for performance tracking
    mutable std::atomic<uint64_t> m_hits;
    mutable std::atomic<uint64_t> m_misses;

public:
    // Singleton instance getter
    static ShardedGdprMetadataCache<T>& get_instance(size_t capacity = (1 << 16)) {
      static ShardedGdprMetadataCache<T> cache_instance(capacity);
      return cache_instance;
    }

    explicit ShardedGdprMetadataCache(size_t total_capacity) 
      : m_hits(0), 
        m_misses(0),
        max_size_per_shard((total_capacity + NUM_SHARDS - 1) / NUM_SHARDS) {}
    
    // Get metadata - returns shared_ptr to avoid copying
    std::shared_ptr<const T> cache_get(std::string_view key) {

      auto& shard = shards[get_shard_index(key)];
      std::shared_lock<std::shared_mutex> lock(shard.mutex);
      
      auto it = shard.entries.find(std::string(key));
      if (it != shard.entries.end()) {
        #ifdef CACHE_STATS
        m_hits.fetch_add(1, std::memory_order_relaxed);
        #endif
        return it->second.data;
      }

      #ifdef CACHE_STATS
      m_misses.fetch_add(1, std::memory_order_relaxed);
      #endif
      
      return nullptr;
    }
    
    // Put metadata - moves data to avoid copying
    void cache_put(std::string_view key, T&& metadata) {

      auto& shard = shards[get_shard_index(key)];
      std::unique_lock<std::shared_mutex> lock(shard.mutex);
      
      std::string key_str(key);
      auto it = shard.entries.find(key_str);
      
      if (it != shard.entries.end()) {
        // Update existing entry
        it->second.data = std::make_shared<const T>(std::move(metadata));
      } else {
        // Add new entry, evict if necessary
        if (shard.size.load(std::memory_order_relaxed) >= max_size_per_shard) {
          random_evict_from_shard(shard);  // O(1) random eviction
        }
        
        CacheEntry entry;
        entry.data = std::make_shared<const T>(std::move(metadata));
        
        shard.entries.emplace(std::move(key_str), std::move(entry));
        shard.size.fetch_add(1, std::memory_order_relaxed);
      }
    }
    
    // Remove from cache
    bool cache_remove(std::string_view key) {

        auto& shard = shards[get_shard_index(key)];
        std::unique_lock<std::shared_mutex> lock(shard.mutex);  // Exclusive lock
        
        auto it = shard.entries.find(std::string(key));
        if (it != shard.entries.end()) {
            shard.entries.erase(it);
            shard.size.fetch_sub(1, std::memory_order_relaxed);
            return true;
        }
        return false;
    }
    
    // Statistics
    size_t total_size() const {
      size_t total = 0;
      for (const auto& shard : shards) {
        total += shard.size.load(std::memory_order_relaxed);
      }
      return total;
    }
    
    size_t capacity() const noexcept {
      return max_size_per_shard * NUM_SHARDS;
    }

    #ifdef CACHE_STATS
    [[nodiscard]] auto cache_hit_rate() const -> double {
      uint64_t hits = m_hits.load(std::memory_order_relaxed);
      uint64_t misses = m_misses.load(std::memory_order_relaxed);
      uint64_t total = hits + misses;
      return total > 0 ? static_cast<double>(hits) / total : 0.0;
    }

    [[nodiscard]] auto cache_hits() const -> uint64_t {
      return m_hits.load(std::memory_order_relaxed);
    }

    [[nodiscard]] auto cache_misses() const -> uint64_t {
      return m_misses.load(std::memory_order_relaxed);
    }
    #endif

private:
    // O(1) random eviction - much faster than LFU
    void random_evict_from_shard(Shard& shard) {
      
      if (shard.entries.empty()) return;
      
      // Generate random index
      std::uniform_int_distribution<size_t> dist(0, shard.entries.size() - 1);
      size_t random_index = dist(shard.rng);
      
      // Find iterator at random position
      auto it = shard.entries.begin();
      std::advance(it, random_index);
      
      shard.entries.erase(it);
      shard.size.fetch_sub(1, std::memory_order_relaxed);
    }
};

using GdprMetadataCache = ShardedGdprMetadataCache<std::string>;

} // namespace controller
