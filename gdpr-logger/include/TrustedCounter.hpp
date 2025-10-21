#ifndef TRUSTED_COUNTER_HPP
#define TRUSTED_COUNTER_HPP

#include <unordered_map>
#include <mutex>
#include <atomic>
#include <string>
#include <memory>
#include <functional>
#include <array>

class TrustedCounter {
public:
    // Number of shards - should be a power of 2 for better hashing
    // Can be tuned based on expected concurrency level
    static constexpr size_t NUM_SHARDS = 64;
    
    TrustedCounter() = default;
    ~TrustedCounter() = default;

    uint32_t getNextCounterForKey(const std::string& key);
    uint32_t getCurrentCounterForKey(const std::string& key) const;
    void resetCounterForKey(const std::string& key);
    void clearAllCounters();

private:
    // Shard structure to group counters and their mutex
    struct Shard {
        mutable std::mutex mutex;
        std::unordered_map<std::string, std::atomic<uint32_t>> counters;
    };

    // Array of shards
    std::array<Shard, NUM_SHARDS> m_shards;

    // Hash function to determine which shard a key belongs to
    size_t getShardIndex(const std::string& key) const {
        return std::hash<std::string>{}(key) & (NUM_SHARDS - 1);
    }
};

#endif
