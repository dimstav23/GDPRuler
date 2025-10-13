#include <cassert>
#include <iostream>
#include <thread>
#include <vector>
#include <cstring>
#include <bitset>
#include "gdpr_index/index_manager.hpp"
#include "gdpr_metadata.hpp"

// ============================================================================
// Helper Functions for Creating Realistic GDPR Metadata
// ============================================================================

// Convert bitset to compact byte representation
template<size_t N>
std::string bitset_to_bytes(const std::bitset<N>& bits) {
    constexpr size_t num_bytes = (N + 7) / 8;
    std::string result(num_bytes, '\0');
    
    for (size_t i = 0; i < N; ++i) {
        if (bits.test(i)) {
            result[i / 8] |= (1 << (i % 8));
        }
    }
    
    return result;
}

// Create complete GDPR metadata value
struct GDPRMetadata {
    size_t owner_bit = 0;           // Single bit: which user owns this
    std::bitset<controller::num_purposes> purpose;
    std::bitset<controller::num_purposes> objection;
    size_t origin_bit = 0;          // Single bit: where data came from
    std::bitset<controller::num_users> share;
    uint64_t expiration_time = 0;
    bool encryption = false;
    bool monitor = false;
    std::string actual_data;
    
    std::string serialize() const {
        std::string result;
        
        // 1. Create and write header
        controller::metadata_header header;
        header.user_bytes = (controller::num_users + 7) / 8;
        header.purpose_bytes = (controller::num_purposes + 7) / 8;
        header.origin_bytes = (controller::num_origins + 7) / 8;
        header.flags = (encryption ? 1 : 0) | (monitor ? 2 : 0);
        header.expiration_time = expiration_time;
        
        result.append(reinterpret_cast<const char*>(&header), sizeof(header));
        
        // 2. Write owner bitmap (only 1 bit set)
        std::bitset<controller::num_users> owner_bitmap;
        owner_bitmap.set(owner_bit);
        result.append(bitset_to_bytes(owner_bitmap));
        
        // 3. Write purpose bitmap (can have multiple bits)
        result.append(bitset_to_bytes(purpose));
        
        // 4. Write objection bitmap
        result.append(bitset_to_bytes(objection));
        
        // 5. Write origin bitmap (only 1 bit set)
        std::bitset<controller::num_origins> origin_bitmap;
        origin_bitmap.set(origin_bit);
        result.append(bitset_to_bytes(origin_bitmap));
        
        // 6. Write share bitmap (can have multiple bits)
        result.append(bitset_to_bytes(share));
        
        // 7. Append actual data
        result.append(actual_data);
        
        return result;
    }
};

// Helper to create test metadata
GDPRMetadata create_test_metadata(
    size_t owner,
    const std::vector<size_t>& purposes,
    const std::vector<size_t>& objections,
    size_t origin,
    const std::vector<size_t>& shared_with,
    uint64_t expiration = 0,
    const std::string& data = "test_data")
{
    GDPRMetadata meta;
    meta.owner_bit = owner;
    meta.origin_bit = origin;
    meta.expiration_time = expiration;
    meta.actual_data = data;
    
    for (size_t p : purposes) {
        meta.purpose.set(p);
    }
    
    for (size_t o : objections) {
        meta.objection.set(o);
    }
    
    for (size_t s : shared_with) {
        meta.share.set(s);
    }
    
    return meta;
}

// ============================================================================
// Test Cases
// ============================================================================

void test_basic_operations() {
    std::cout << "Test: Basic insert/delete operations... ";
    
    controller::IndexConfig config;
    config.enable_owner_index = true;
    config.num_shards = 4;
    controller::IndexManager manager(config);
    
    // Test insert
    auto meta = create_test_metadata(5, {1, 3}, {}, 0, {});
    manager.on_put("key1", meta.serialize());
    assert(manager.total_indexed_keys() == 1);
    
    // Test delete
    manager.on_delete("key1");
    assert(manager.total_indexed_keys() == 0);
    
    std::cout << "PASSED ✓" << std::endl;
}

void test_owner_queries() {
    std::cout << "Test: Owner queries (single-bit bitmap)... ";
    
    controller::IndexConfig config;
    config.enable_owner_index = true;
    config.num_shards = 4;
    controller::IndexManager manager(config);
    
    // Create data owned by different users
    auto meta1 = create_test_metadata(0, {1}, {}, 0, {});
    auto meta2 = create_test_metadata(1, {2}, {}, 1, {});
    auto meta3 = create_test_metadata(0, {3}, {}, 2, {});
    auto meta4 = create_test_metadata(2, {4}, {}, 3, {});
    
    manager.on_put("alice_key1", meta1.serialize());
    manager.on_put("bob_key1", meta2.serialize());
    manager.on_put("alice_key2", meta3.serialize());
    manager.on_put("charlie_key1", meta4.serialize());
    
    // Query for user 0's data (Alice)
    auto results = manager.find_by_owner(0);
    assert(results.size() == 2);
    
    // Verify correct keys returned
    bool found_key1 = false, found_key2 = false;
    for (const auto& key : results) {
        if (key == "alice_key1") found_key1 = true;
        if (key == "alice_key2") found_key2 = true;
    }
    assert(found_key1 && found_key2);
    
    std::cout << "PASSED ✓" << std::endl;
}

void test_purpose_queries() {
    std::cout << "Test: Purpose queries (multi-bit bitmap)... ";
    
    controller::IndexConfig config;
    config.enable_purpose_index = true;
    config.num_shards = 4;
    controller::IndexManager manager(config);
    
    // Create data with different purposes
    // Key1: purposes {1, 3, 5}
    auto meta1 = create_test_metadata(0, {1, 3, 5}, {}, 0, {});
    manager.on_put("key1", meta1.serialize());
    
    // Key2: purposes {3, 7}
    auto meta2 = create_test_metadata(1, {3, 7}, {}, 1, {});
    manager.on_put("key2", meta2.serialize());
    
    // Key3: purposes {5, 9}
    auto meta3 = create_test_metadata(2, {5, 9}, {}, 2, {});
    manager.on_put("key3", meta3.serialize());
    
    // Query: Find keys with purpose 3
    std::bitset<controller::num_purposes> query_purpose3;
    query_purpose3.set(3);
    auto results = manager.find_by_purpose(query_purpose3);
    
    // Should find key1 and key2 (both have purpose 3)
    assert(results.size() == 2);
    
    // Query: Find keys with purpose 3 OR 5
    std::bitset<controller::num_purposes> query_purpose_3_or_5;
    query_purpose_3_or_5.set(3);
    query_purpose_3_or_5.set(5);
    auto results_or = manager.find_by_purpose(query_purpose_3_or_5);
    
    // Should find all 3 keys (all have either 3 or 5 or both)
    assert(results_or.size() == 3);
    
    std::cout << "PASSED ✓" << std::endl;
}

void test_expiration_range_queries() {
    std::cout << "Test: Expiration range queries (B+ tree)... ";
    
    controller::IndexConfig config;
    config.enable_expiration_index = true;
    config.num_shards = 4;
    controller::IndexManager manager(config);
    
    // Create data with different expiration times
    uint64_t now = 1700000000;
    auto meta1 = create_test_metadata(0, {1}, {}, 0, {}, now + 3600);    // +1 hour
    auto meta2 = create_test_metadata(1, {2}, {}, 1, {}, now + 86400);   // +1 day
    auto meta3 = create_test_metadata(2, {3}, {}, 2, {}, now - 3600);    // Already expired
    auto meta4 = create_test_metadata(3, {4}, {}, 3, {}, now + 604800);  // +1 week
    
    manager.on_put("expires_1h", meta1.serialize());
    manager.on_put("expires_1d", meta2.serialize());
    manager.on_put("expired", meta3.serialize());
    manager.on_put("expires_1w", meta4.serialize());
    
    // Find keys expiring in next 2 hours
    uint64_t threshold = now + 7200;
    auto results = manager.find_by_expiration_range(0, threshold);
    
    // Should find: expired (in past) and expires_1h
    assert(results.size() == 2);
    
    // Find keys expiring in next 2 days
    auto results_2d = manager.find_by_expiration_range(0, now + 2 * 86400);
    
    // Should find: expired, expires_1h, expires_1d
    assert(results_2d.size() == 3);
    
    std::cout << "PASSED ✓" << std::endl;
}

void test_combined_queries() {
    std::cout << "Test: Combined multi-criteria queries (intersection)... ";
    
    controller::IndexConfig config;
    config.enable_owner_index = true;
    config.enable_purpose_index = true;
    config.enable_expiration_index = true;
    config.num_shards = 4;
    controller::IndexManager manager(config);
    
    uint64_t now = 1700000000;
    
    // User 0, purpose 1, expires soon
    auto meta1 = create_test_metadata(0, {1, 2}, {}, 0, {}, now + 3600);
    manager.on_put("user0_p1_soon", meta1.serialize());
    
    // User 0, purpose 1, expires later
    auto meta2 = create_test_metadata(0, {1, 3}, {}, 1, {}, now + 604800);
    manager.on_put("user0_p1_later", meta2.serialize());
    
    // User 1, purpose 1, expires soon
    auto meta3 = create_test_metadata(1, {1}, {}, 2, {}, now + 3600);
    manager.on_put("user1_p1_soon", meta3.serialize());
    
    // User 0, purpose 3, expires soon
    auto meta4 = create_test_metadata(0, {3}, {}, 3, {}, now + 3600);
    manager.on_put("user0_p3_soon", meta4.serialize());
    
    // Query: Find keys owned by user 0 with purpose 1 expiring in next 2 hours
    std::bitset<controller::num_purposes> purpose_query;
    purpose_query.set(1);
    
    auto results = manager.find_keys(
        std::make_optional<size_t>(0),           // owner = 0
        std::make_optional(purpose_query),        // purpose = 1
        std::make_optional<uint64_t>(now + 7200) // expiration < now + 2h
    );
    
    // Should find only: user0_p1_soon
    // (user0_p1_later expires too late, user1_p1_soon is wrong owner, user0_p3_soon is wrong purpose)
    assert(results.size() == 1);
    assert(results[0] == "user0_p1_soon");
    
    std::cout << "PASSED ✓" << std::endl;
}

void test_update_metadata() {
    std::cout << "Test: Update with metadata changes... ";
    
    controller::IndexConfig config;
    config.enable_owner_index = true;
    config.enable_purpose_index = true;
    config.num_shards = 4;
    controller::IndexManager manager(config);
    
    // Initial: User 0, purpose 1
    auto meta1 = create_test_metadata(0, {1}, {}, 0, {});
    manager.on_put("data_key", meta1.serialize());
    
    // Verify initial state
    auto results1 = manager.find_by_owner(0);
    assert(results1.size() == 1);
    
    // Update: Change to user 1, purposes 2 and 3
    auto meta2 = create_test_metadata(1, {2, 3}, {}, 0, {});
    manager.on_put("data_key", meta2.serialize());
    
    // Old owner should have no results
    auto old_results = manager.find_by_owner(0);
    assert(old_results.empty());
    
    // New owner should have 1 result
    auto new_results = manager.find_by_owner(1);
    assert(new_results.size() == 1);
    
    // Should be findable by new purposes
    std::bitset<controller::num_purposes> purpose2;
    purpose2.set(2);
    auto purpose_results = manager.find_by_purpose(purpose2);
    assert(purpose_results.size() == 1);
    
    std::cout << "PASSED ✓" << std::endl;
}

void test_concurrent_access() {
    std::cout << "Test: Concurrent access with sharding... " << std::flush;
    
    controller::IndexConfig config;
    config.enable_owner_index = true;
    config.enable_purpose_index = true;
    config.num_shards = 16;
    controller::IndexManager manager(config);
    
    const size_t num_threads = 8;
    const size_t ops_per_thread = 100;
    
    {  // Scope for threads
        std::vector<std::thread> threads;
        threads.reserve(num_threads);
        
        for (size_t t = 0; t < num_threads; ++t) {
            threads.emplace_back([&manager, t, ops_per_thread]() {
                for (size_t i = 0; i < ops_per_thread; ++i) {
                    std::string key = "thread" + std::to_string(t) + "_key" + std::to_string(i);
                    auto meta = create_test_metadata(t, {i % 10}, {}, t % 5, {});
                    manager.on_put(key, meta.serialize());
                }
            });
        }
        
        // Wait for ALL threads to complete before checking
        for (auto& thread : threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
    }  // All threads destroyed here
    
    // Now safe to query
    assert(manager.total_indexed_keys() == num_threads * ops_per_thread);
    
    for (size_t user = 0; user < num_threads; ++user) {
        auto results = manager.find_by_owner(user);
        assert(results.size() == ops_per_thread);
    }
    
    std::cout << "PASSED ✓" << std::endl;
}

void test_memory_efficiency() {
    std::cout << "Test: Memory efficiency (string interning)... ";
    
    controller::IndexConfig config;
    config.enable_owner_index = true;
    config.enable_purpose_index = true;
    config.enable_expiration_index = true;
    config.num_shards = 8;
    controller::IndexManager manager(config);
    
    // Same key updated multiple times - should only store string once
    std::string long_key = "user_profile_data_with_very_long_key_name_to_test_memory_efficiency";
    
    for (size_t i = 0; i < 10; ++i) {
        auto meta = create_test_metadata(i, {i % 5, (i + 1) % 5}, {}, i % 3, {}, 1700000000 + i * 1000);
        manager.on_put(long_key, meta.serialize());
    }
    
    // Despite 10 updates, only 1 key exists
    assert(manager.total_indexed_keys() == 1);
    
    // Should be findable by latest metadata (owner 9)
    auto results = manager.find_by_owner(9);
    assert(results.size() == 1);
    assert(results[0] == long_key);
    
    std::cout << "PASSED ✓" << std::endl;
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "GDPR Index Manager Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Configuration:" << std::endl;
    std::cout << "  - Users: " << controller::num_users << std::endl;
    std::cout << "  - Purposes: " << controller::num_purposes << std::endl;
    std::cout << "  - Origins: " << controller::num_origins << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    test_basic_operations();
    test_owner_queries();
    test_purpose_queries();
    test_expiration_range_queries();
    test_combined_queries();
    test_update_metadata();
    test_concurrent_access();
    test_memory_efficiency();
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "✓ All tests PASSED!" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    return 0;
}
