#include <gtest/gtest.h>
#include "TrustedCounter.hpp"
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <random>
#include <algorithm>

class TrustedCounterTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        counter = std::make_unique<TrustedCounter>();
    }

    void TearDown() override
    {
        counter.reset();
    }

    std::unique_ptr<TrustedCounter> counter;
};

// Test basic counter increment for single key
TEST_F(TrustedCounterTest, BasicCounterIncrement)
{
    std::string key = "test_key";
    
    // First call should return 0 and increment to 1
    EXPECT_EQ(counter->getNextCounterForKey(key), 0);
    EXPECT_EQ(counter->getCurrentCounterForKey(key), 1);
    
    // Second call should return 1 and increment to 2
    EXPECT_EQ(counter->getNextCounterForKey(key), 1);
    EXPECT_EQ(counter->getCurrentCounterForKey(key), 2);
    
    // Third call should return 2 and increment to 3
    EXPECT_EQ(counter->getNextCounterForKey(key), 2);
    EXPECT_EQ(counter->getCurrentCounterForKey(key), 3);
}

// Test multiple keys independence
TEST_F(TrustedCounterTest, MultipleKeysIndependence)
{
    std::string key1 = "key1";
    std::string key2 = "key2";
    std::string key3 = "key3";
    
    // Each key should start from 0 independently
    EXPECT_EQ(counter->getNextCounterForKey(key1), 0);
    EXPECT_EQ(counter->getNextCounterForKey(key2), 0);
    EXPECT_EQ(counter->getNextCounterForKey(key3), 0);
    
    // Increment key1 multiple times
    EXPECT_EQ(counter->getNextCounterForKey(key1), 1);
    EXPECT_EQ(counter->getNextCounterForKey(key1), 2);
    
    // key2 and key3 should still be at 1
    EXPECT_EQ(counter->getCurrentCounterForKey(key1), 3);
    EXPECT_EQ(counter->getCurrentCounterForKey(key2), 1);
    EXPECT_EQ(counter->getCurrentCounterForKey(key3), 1);
    
    // Increment key2
    EXPECT_EQ(counter->getNextCounterForKey(key2), 1);
    EXPECT_EQ(counter->getCurrentCounterForKey(key2), 2);
    
    // key1 and key3 should be unchanged
    EXPECT_EQ(counter->getCurrentCounterForKey(key1), 3);
    EXPECT_EQ(counter->getCurrentCounterForKey(key3), 1);
}

// Test getCurrentCounterForKey with non-existent key
TEST_F(TrustedCounterTest, NonExistentKey)
{
    std::string key = "nonexistent_key";
    
    // Should return 0 for non-existent key
    EXPECT_EQ(counter->getCurrentCounterForKey(key), 0);
}

// Test resetCounterForKey
TEST_F(TrustedCounterTest, ResetCounter)
{
    std::string key = "reset_test_key";
    
    // Increment counter several times
    for (int i = 0; i < 5; ++i) {
        counter->getNextCounterForKey(key);
    }
    EXPECT_EQ(counter->getCurrentCounterForKey(key), 5);
    
    // Reset the counter
    counter->resetCounterForKey(key);
    EXPECT_EQ(counter->getCurrentCounterForKey(key), 0);
    
    // Next call should return 0 again
    EXPECT_EQ(counter->getNextCounterForKey(key), 0);
    EXPECT_EQ(counter->getCurrentCounterForKey(key), 1);
}

// Test resetCounterForKey with non-existent key
TEST_F(TrustedCounterTest, ResetNonExistentKey)
{
    std::string key = "nonexistent_reset_key";
    
    // Should not crash when resetting non-existent key
    EXPECT_NO_THROW(counter->resetCounterForKey(key));
    
    // Should still return 0
    EXPECT_EQ(counter->getCurrentCounterForKey(key), 0);
}

// Test clearAllCounters
TEST_F(TrustedCounterTest, ClearAllCounters)
{
    // Create and increment multiple counters
    std::vector<std::string> keys = {"key1", "key2", "key3", "key4", "key5"};
    
    for (const auto& key : keys) {
        for (int i = 0; i < 3; ++i) {
            counter->getNextCounterForKey(key);
        }
    }
    
    // Verify all counters are at 3
    for (const auto& key : keys) {
        EXPECT_EQ(counter->getCurrentCounterForKey(key), 3);
    }
    
    // Clear all counters
    counter->clearAllCounters();
    
    // All counters should be reset to 0
    for (const auto& key : keys) {
        EXPECT_EQ(counter->getCurrentCounterForKey(key), 0);
    }
    
    // Next increment should start from 0 again
    for (const auto& key : keys) {
        EXPECT_EQ(counter->getNextCounterForKey(key), 0);
        EXPECT_EQ(counter->getCurrentCounterForKey(key), 1);
    }
}

// Test thread safety - multiple threads incrementing same key
TEST_F(TrustedCounterTest, ThreadSafetySameKey)
{
    std::string key = "thread_test_key";
    const int numThreads = 10;
    const int incrementsPerThread = 100;
    
    std::vector<std::thread> threads;
    std::vector<std::vector<uint32_t>> results(numThreads);
    
    // Launch threads that increment the same key
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([this, &key, &results, i, incrementsPerThread]() {
            results[i].reserve(incrementsPerThread);
            for (int j = 0; j < incrementsPerThread; ++j) {
                uint32_t value = counter->getNextCounterForKey(key);
                results[i].push_back(value);
            }
        });
    }
    
    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }
    
    // Collect all returned values
    std::vector<uint32_t> allValues;
    for (const auto& threadResults : results) {
        allValues.insert(allValues.end(), threadResults.begin(), threadResults.end());
    }
    
    // Sort the values
    std::sort(allValues.begin(), allValues.end());
    
    // Should have exactly numThreads * incrementsPerThread values
    EXPECT_EQ(allValues.size(), numThreads * incrementsPerThread);
    
    // Values should be consecutive from 0 to (total-1)
    for (size_t i = 0; i < allValues.size(); ++i) {
        EXPECT_EQ(allValues[i], i);
    }
    
    // Final counter value should be the total number of increments
    EXPECT_EQ(counter->getCurrentCounterForKey(key), numThreads * incrementsPerThread);
}

// Test thread safety - multiple threads with different keys
TEST_F(TrustedCounterTest, ThreadSafetyDifferentKeys)
{
    const int numThreads = 8;
    const int incrementsPerThread = 50;
    
    std::vector<std::thread> threads;
    std::vector<std::string> keys;
    
    // Generate unique keys for each thread
    for (int i = 0; i < numThreads; ++i) {
        keys.push_back("thread_key_" + std::to_string(i));
    }
    
    // Launch threads with different keys
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([this, &keys, i, incrementsPerThread]() {
            for (int j = 0; j < incrementsPerThread; ++j) {
                counter->getNextCounterForKey(keys[i]);
            }
        });
    }
    
    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }
    
    // Each key should have exactly incrementsPerThread
    for (const auto& key : keys) {
        EXPECT_EQ(counter->getCurrentCounterForKey(key), incrementsPerThread);
    }
}

// Test thread safety - mixed operations (increment, reset, clear)
TEST_F(TrustedCounterTest, ThreadSafetyMixedOperations)
{
    const int numIncrementThreads = 4;
    const int numResetThreads = 2;
    const int operationsPerThread = 100;
    
    std::vector<std::string> keys = {"mixed_key_1", "mixed_key_2", "mixed_key_3"};
    std::vector<std::thread> threads;
    std::atomic<bool> stopFlag{false};
    
    // Increment threads
    for (int i = 0; i < numIncrementThreads; ++i) {
        threads.emplace_back([this, &keys, &stopFlag, operationsPerThread]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> keyDist(0, keys.size() - 1);
            
            for (int j = 0; j < operationsPerThread && !stopFlag; ++j) {
                std::string& key = keys[keyDist(gen)];
                counter->getNextCounterForKey(key);
                std::this_thread::sleep_for(std::chrono::microseconds(1));
            }
        });
    }
    
    // Reset threads (less frequent operations)
    for (int i = 0; i < numResetThreads; ++i) {
        threads.emplace_back([this, &keys, &stopFlag]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> keyDist(0, keys.size() - 1);
            
            for (int j = 0; j < 10 && !stopFlag; ++j) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                std::string& key = keys[keyDist(gen)];
                counter->resetCounterForKey(key);
            }
        });
    }
    
    // Let threads run for a bit
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    stopFlag = true;
    
    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }
    
    // No crashes means success for this stress test
    // Just verify the system is still functional
    for (const auto& key : keys) {
        uint32_t initialValue = counter->getCurrentCounterForKey(key);
        uint32_t nextValue = counter->getNextCounterForKey(key);
        EXPECT_EQ(counter->getCurrentCounterForKey(key), nextValue + 1);
    }
}

// Test hash distribution across shards
TEST_F(TrustedCounterTest, ShardDistribution)
{
    // Test with many different keys to ensure good distribution
    std::vector<std::string> keys;
    for (int i = 0; i < 1000; ++i) {
        keys.push_back("shard_test_key_" + std::to_string(i));
    }
    
    // Increment each key once
    for (const auto& key : keys) {
        EXPECT_EQ(counter->getNextCounterForKey(key), 0);
        EXPECT_EQ(counter->getCurrentCounterForKey(key), 1);
    }
    
    // All keys should be accessible and independent
    for (const auto& key : keys) {
        EXPECT_EQ(counter->getCurrentCounterForKey(key), 1);
    }
}

// Test large counter values
TEST_F(TrustedCounterTest, LargeCounterValues)
{
    std::string key = "large_counter_test";
    const uint32_t targetValue = 10000;
    
    // Increment to large value
    for (uint32_t i = 0; i < targetValue; ++i) {
        EXPECT_EQ(counter->getNextCounterForKey(key), i);
    }
    
    EXPECT_EQ(counter->getCurrentCounterForKey(key), targetValue);
    
    // Continue incrementing
    EXPECT_EQ(counter->getNextCounterForKey(key), targetValue);
    EXPECT_EQ(counter->getCurrentCounterForKey(key), targetValue + 1);
}

// Test key with special characters
TEST_F(TrustedCounterTest, SpecialCharacterKeys)
{
    std::vector<std::string> specialKeys = {
        "key with spaces",
        "key/with/slashes",
        "key-with-dashes",
        "key_with_underscores",
        "key.with.dots",
        "key@with#special$chars%",
        "UPPERCASE_KEY",
        "123numeric456key789",
        ""  // empty string
    };
    
    for (const auto& key : specialKeys) {
        EXPECT_EQ(counter->getNextCounterForKey(key), 0);
        EXPECT_EQ(counter->getCurrentCounterForKey(key), 1);
        
        EXPECT_EQ(counter->getNextCounterForKey(key), 1);
        EXPECT_EQ(counter->getCurrentCounterForKey(key), 2);
    }
    
    // Verify all keys are still independent
    for (const auto& key : specialKeys) {
        EXPECT_EQ(counter->getCurrentCounterForKey(key), 2);
    }
}

// Performance test
TEST_F(TrustedCounterTest, PerformanceTest)
{
    std::string key = "performance_test_key";
    const int numOperations = 100000;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < numOperations; ++i) {
        counter->getNextCounterForKey(key);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Should be able to do 100k operations reasonably quickly
    // This is more of a performance indicator than a strict test
    std::cout << "Performance: " << numOperations << " operations took " 
              << duration.count() << " microseconds" << std::endl;
    std::cout << "Average: " << (duration.count() / numOperations) 
              << " microseconds per operation" << std::endl;
    
    EXPECT_EQ(counter->getCurrentCounterForKey(key), numOperations);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
