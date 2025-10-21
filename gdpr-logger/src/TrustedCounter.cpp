#include "TrustedCounter.hpp"
#include <iostream>

uint32_t TrustedCounter::getNextCounterForKey(const std::string& key) {
    size_t shardIndex = getShardIndex(key);
    Shard& shard = m_shards[shardIndex];
    
    std::lock_guard<std::mutex> lock(shard.mutex);
    
    // Try to find existing counter
    auto it = shard.counters.find(key);
    if (it != shard.counters.end()) {
        // Key exists - atomic increment and return new value
        return it->second.fetch_add(1, std::memory_order_relaxed);
    }
    
    // Key doesn't exist - create it and return 0 for first batch
    auto [inserted_it, inserted] = shard.counters.try_emplace(key, 0);
    return inserted_it->second.fetch_add(1, std::memory_order_relaxed);
}

uint32_t TrustedCounter::getCurrentCounterForKey(const std::string& key) const {
    size_t shardIndex = getShardIndex(key);
    const Shard& shard = m_shards[shardIndex];
    
    std::lock_guard<std::mutex> lock(shard.mutex);
    
    auto it = shard.counters.find(key);
    if (it != shard.counters.end()) {
        return it->second.load(std::memory_order_relaxed);
    }
    return 0; // Key doesn't exist yet
}

void TrustedCounter::resetCounterForKey(const std::string& key) {
    size_t shardIndex = getShardIndex(key);
    Shard& shard = m_shards[shardIndex];
    
    std::lock_guard<std::mutex> lock(shard.mutex);
    
    auto it = shard.counters.find(key);
    if (it != shard.counters.end()) {
        it->second.store(0, std::memory_order_relaxed);
    }
}

void TrustedCounter::clearAllCounters() {
    // Lock all shards to ensure consistency
    std::array<std::unique_lock<std::mutex>, NUM_SHARDS> locks;
    for (size_t i = 0; i < NUM_SHARDS; ++i) {
        locks[i] = std::unique_lock<std::mutex>(m_shards[i].mutex);
    }
    
    // Clear all shards
    for (auto& shard : m_shards) {
        shard.counters.clear();
    }
}
