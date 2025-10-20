#include <iostream>
#include <chrono>
#include <random>
#include <thread>
#include <vector>
#include <cstring>
#include <iomanip>
#include <algorithm>
#include "gdpr_index/index_manager.hpp"
#include "gdpr_metadata.hpp"

// ============================================================================
// Benchmark Utilities
// ============================================================================

// Helper to create test metadata (same as in index_test.cpp)
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

struct GDPRMetadata {
    size_t owner_bit = 0;
    std::bitset<controller::num_purposes> purpose;
    std::bitset<controller::num_purposes> objection;
    size_t origin_bit = 0;
    std::bitset<controller::num_users> share;
    uint64_t expiration_time = 0;
    bool encryption = false;
    bool monitor = false;
    std::string actual_data;
    
    std::string serialize() const {
        std::string result;
        
        controller::metadata_header header;
        header.user_bytes = (controller::num_users + 7) / 8;
        header.purpose_bytes = (controller::num_purposes + 7) / 8;
        header.origin_bytes = (controller::num_origins + 7) / 8;
        header.flags = (encryption ? 1 : 0) | (monitor ? 2 : 0);
        header.expiration_time = expiration_time;
        
        result.append(reinterpret_cast<const char*>(&header), sizeof(header));
        
        std::bitset<controller::num_users> owner_bitmap;
        owner_bitmap.set(owner_bit);
        result.append(bitset_to_bytes(owner_bitmap));
        
        result.append(bitset_to_bytes(purpose));
        result.append(bitset_to_bytes(objection));
        
        std::bitset<controller::num_origins> origin_bitmap;
        origin_bitmap.set(origin_bit);
        result.append(bitset_to_bytes(origin_bitmap));
        
        result.append(bitset_to_bytes(share));
        result.append(actual_data);
        
        return result;
    }
};

GDPRMetadata create_test_metadata(
    size_t owner,
    const std::vector<size_t>& purposes,
    size_t origin,
    uint64_t expiration = 0,
    const std::string& data = "benchmark_test_data")
{
    GDPRMetadata meta;
    
    // Bounds checking for owner (must be < 128)
    meta.owner_bit = owner % controller::num_users;  // Wrap to valid range
    
    // Bounds checking for origin (must be < 128)
    meta.origin_bit = origin % controller::num_origins;  // Wrap to valid range
    
    meta.expiration_time = expiration;
    meta.actual_data = data;
    
    // Bounds checking for purposes (must be < 128)
    for (size_t p : purposes) {
        if (p < controller::num_purposes) {
            meta.purpose.set(p);
        }
        // Silently skip invalid purpose values
    }
    
    return meta;
}

// Timing helper
class Timer {
public:
    Timer() : start_(std::chrono::steady_clock::now()) {}
    
    double elapsed_ms() const {
        auto end = std::chrono::steady_clock::now();
        return std::chrono::duration<double, std::milli>(end - start_).count();
    }
    
    double elapsed_us() const {
        auto end = std::chrono::steady_clock::now();
        return std::chrono::duration<double, std::micro>(end - start_).count();
    }
    
private:
    std::chrono::steady_clock::time_point start_;
};

void print_separator() {
    std::cout << "========================================" << std::endl;
}

void print_result(const std::string& name, size_t operations, double time_ms) {
    double ops_per_sec = (operations * 1000.0) / time_ms;
    std::cout << std::left << std::setw(40) << name 
              << std::right << std::setw(10) << std::fixed << std::setprecision(2) << time_ms << " ms"
              << std::setw(15) << std::fixed << std::setprecision(0) << ops_per_sec << " ops/sec" 
              << std::endl;
}

// ============================================================================
// Benchmark 1: Insert Performance
// ============================================================================

void benchmark_insert_performance() {
    std::cout << "\n=== Insert Performance ===" << std::endl;
    print_separator();
    
    controller::IndexConfig config;
    config.enable_owner_index = true;
    config.enable_purpose_index = true;
    config.enable_expiration_index = true;
    config.num_shards = 64;
    controller::IndexManager manager(config);
    
    const size_t num_keys = 100000;
    const uint64_t base_time = 1700000000;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    
    // BETTER DISTRIBUTION: 1000 users, 64 purposes
    std::uniform_int_distribution<size_t> owner_dist(0, 999);
    std::uniform_int_distribution<size_t> purpose_dist(0, 63);
    std::uniform_int_distribution<uint64_t> time_dist(0, 365 * 86400);
    
    Timer timer;
    
    for (size_t i = 0; i < num_keys; ++i) {
        std::string key = "user_key_" + std::to_string(i);
        size_t owner = owner_dist(gen);
        size_t purpose1 = purpose_dist(gen);
        size_t purpose2 = (purpose1 + 1) % 64;
        uint64_t exp_time = base_time + time_dist(gen);
        
        auto meta = create_test_metadata(owner, {purpose1, purpose2}, owner % 10, exp_time);
        manager.on_put(key, meta.serialize());
    }
    
    double elapsed = timer.elapsed_ms();
    print_result("Insert 100k keys (3 indexes)", num_keys, elapsed);
    
    std::cout << "  Total indexed: " << manager.total_indexed_keys() << " keys" << std::endl;
    std::cout << "  Avg per insert: " << std::fixed << std::setprecision(2) 
              << (elapsed * 1000.0 / num_keys) << " μs" << std::endl;
}

// ============================================================================
// Benchmark 2: Query Performance by Index Type
// ============================================================================

void benchmark_query_performance() {
    std::cout << "\n=== Query Performance by Index Type ===" << std::endl;
    print_separator();
    
    controller::IndexConfig config;
    config.enable_owner_index = true;
    config.enable_purpose_index = true;
    config.enable_expiration_index = true;
    config.num_shards = 64;
    controller::IndexManager manager(config);
    
    const size_t num_keys = 100000;
    const uint64_t base_time = 1700000000;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> owner_dist(0, 999);
    std::uniform_int_distribution<size_t> purpose_dist(0, 63);
    std::uniform_int_distribution<uint64_t> time_dist(0, 365 * 86400);
    
    std::cout << "Inserting " << num_keys << " keys..." << std::flush;
    
    for (size_t i = 0; i < num_keys; ++i) {
        std::string key = "key_" + std::to_string(i);
        size_t owner = owner_dist(gen);
        size_t purpose1 = purpose_dist(gen);
        size_t purpose2 = (purpose1 + 1) % 64;
        uint64_t exp_time = base_time + time_dist(gen);
        
        auto meta = create_test_metadata(owner, {purpose1, purpose2}, owner % 10, exp_time);
        manager.on_put(key, meta.serialize());
        
        if ((i + 1) % 10000 == 0) {
            std::cout << "." << std::flush;
        }
    }
    
    std::cout << " Done!" << std::endl;
    
    const size_t num_queries = 10000;
    
    // Benchmark 1: Owner queries
    {
        std::cout << "Running " << num_queries << " owner queries..." << std::flush;
        Timer timer;
        size_t total_results = 0;
        
        for (size_t i = 0; i < num_queries; ++i) {
            size_t owner = i % 1000;
            auto results = manager.find_by_owner(owner);
            total_results += results.size();
            
            if ((i + 1) % 1000 == 0) {
                std::cout << "." << std::flush;
            }
        }
        std::cout << " Done!" << std::endl;
        
        double elapsed = timer.elapsed_ms();
        print_result("Owner queries (Hashmap)", num_queries, elapsed);
        std::cout << "  Avg latency: " << std::fixed << std::setprecision(0) 
                  << (elapsed * 1000.0 / num_queries) << " μs" << std::endl;
        std::cout << "  Avg results: " << (total_results / num_queries) << " keys" << std::endl;
    }
    
    // Benchmark 2a: Purpose queries (SINGLE BIT - fast path)
    {
        std::cout << "Running " << num_queries << " purpose queries (1 bit)..." << std::flush;
        Timer timer;
        size_t total_results = 0;
        
        for (size_t i = 0; i < num_queries; ++i) {
            std::bitset<controller::num_purposes> purpose_query;
            purpose_query.set(i % 64);  // Only 1 bit set - triggers fast path
            auto results = manager.find_by_purpose(purpose_query);
            total_results += results.size();
            
            if ((i + 1) % 1000 == 0) {
                std::cout << "." << std::flush;
            }
        }
        std::cout << " Done!" << std::endl;
        
        double elapsed = timer.elapsed_ms();
        print_result("Purpose queries (1 bit, fast path)", num_queries, elapsed);
        std::cout << "  Avg latency: " << std::fixed << std::setprecision(0) 
                  << (elapsed * 1000.0 / num_queries) << " μs" << std::endl;
        std::cout << "  Avg results: " << (total_results / num_queries) << " keys" << std::endl;
    }
    
    // Benchmark 2b: Purpose queries (MULTIPLE BITS - deduplication needed)
    {
        std::cout << "Running " << num_queries << " purpose queries (4 bits)..." << std::flush;
        Timer timer;
        size_t total_results = 0;
        
        for (size_t i = 0; i < num_queries; ++i) {
            std::bitset<controller::num_purposes> purpose_query;
            // Set 4 bits - requires hash set for deduplication
            purpose_query.set(i % 64);
            purpose_query.set((i + 1) % 64);
            purpose_query.set((i + 2) % 64);
            purpose_query.set((i + 3) % 64);
            auto results = manager.find_by_purpose(purpose_query);
            total_results += results.size();
            
            if ((i + 1) % 1000 == 0) {
                std::cout << "." << std::flush;
            }
        }
        std::cout << " Done!" << std::endl;
        
        double elapsed = timer.elapsed_ms();
        print_result("Purpose queries (4 bits, with dedup)", num_queries, elapsed);
        std::cout << "  Avg latency: " << std::fixed << std::setprecision(0) 
                  << (elapsed * 1000.0 / num_queries) << " μs" << std::endl;
        std::cout << "  Avg results: " << (total_results / num_queries) << " keys" << std::endl;
    }
    
    // Benchmark 3: Expiration range queries
    {
        std::cout << "Running " << num_queries << " expiration queries..." << std::flush;
        Timer timer;
        size_t total_results = 0;
        
        for (size_t i = 0; i < num_queries; ++i) {
            uint64_t threshold = base_time + (i % 30) * 86400;
            auto results = manager.find_by_expiration_range(0, threshold);
            total_results += results.size();
            
            if ((i + 1) % 1000 == 0) {
                std::cout << "." << std::flush;
            }
        }
        std::cout << " Done!" << std::endl;
        
        double elapsed = timer.elapsed_ms();
        print_result("Expiration range queries (BTree)", num_queries, elapsed);
        std::cout << "  Avg latency: " << std::fixed << std::setprecision(0) 
                  << (elapsed * 1000.0 / num_queries) << " μs" << std::endl;
        std::cout << "  Avg results: " << (total_results / num_queries) << " keys" << std::endl;
    }
}

// ============================================================================
// Benchmark 3: Multi-Criteria Query Performance
// ============================================================================

void benchmark_combined_queries() {
    std::cout << "\n=== Multi-Criteria Query Performance ===" << std::endl;
    print_separator();
    
    controller::IndexConfig config;
    config.enable_owner_index = true;
    config.enable_purpose_index = true;
    config.enable_expiration_index = true;
    config.num_shards = 64;
    controller::IndexManager manager(config);
    
    const size_t num_keys = 100000;
    const uint64_t base_time = 1700000000;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> owner_dist(0, 999);
    std::uniform_int_distribution<size_t> purpose_dist(0, 63);
    
    // Use FIXED expiration time for 30% of keys to ensure overlap
    std::uniform_real_distribution<double> exp_prob(0.0, 1.0);
    const uint64_t common_exp_time = base_time + 15 * 86400;  // 15 days
    
    std::cout << "Inserting 100k keys..." << std::flush;
    
    for (size_t i = 0; i < num_keys; ++i) {
        std::string key = "key_" + std::to_string(i);
        size_t owner = owner_dist(gen);
        size_t purpose1 = purpose_dist(gen);
        size_t purpose2 = (purpose1 + 1) % 64;
        
        // 30% get the common expiration time (ensures overlap)
        uint64_t exp_time = (exp_prob(gen) < 0.3) ? common_exp_time : base_time + (i % 365) * 86400;
        
        auto meta = create_test_metadata(owner, {purpose1, purpose2}, owner % 10, exp_time);
        manager.on_put(key, meta.serialize());
        
        if ((i + 1) % 10000 == 0) {
            std::cout << "." << std::flush;
        }
    }
    std::cout << " Done!" << std::endl;
    
    const size_t num_queries = 10000;
    
    {
        std::cout << "Running 10k combined queries..." << std::flush;
        Timer timer;
        size_t total_results = 0;
        
        for (size_t i = 0; i < num_queries; ++i) {
            size_t owner = i % 1000;
            std::bitset<controller::num_purposes> purpose_query;
            purpose_query.set(i % 64);
            // Query for the common expiration time (where we have overlap)
            uint64_t threshold = common_exp_time;
            
            auto results = manager.find_keys(
                std::make_optional(owner),
                std::make_optional(purpose_query),
                std::make_optional(threshold)
            );
            
            total_results += results.size();
            
            if ((i + 1) % 1000 == 0) {
                std::cout << "." << std::flush;
            }
        }
        std::cout << " Done!" << std::endl;
        
        double elapsed = timer.elapsed_ms();
        print_result("Combined queries (3-way AND)", num_queries, elapsed);
        std::cout << "  Avg latency: " << std::fixed << std::setprecision(0) 
                  << (elapsed * 1000.0 / num_queries) << " μs" << std::endl;
        std::cout << "  Avg results per query: " << (total_results / num_queries) << " keys" << std::endl;
    }
}

// ============================================================================
// Benchmark 4: Concurrent Query Performance
// ============================================================================

void benchmark_concurrent_queries() {
    std::cout << "\n=== Concurrent Query Performance ===" << std::endl;
    print_separator();
    
    controller::IndexConfig config;
    config.enable_owner_index = true;
    config.enable_purpose_index = true;
    config.num_shards = 64;
    controller::IndexManager manager(config);
    
    const size_t num_keys = 100000;
    const uint64_t base_time = 1700000000;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> owner_dist(0, 999);
    std::uniform_int_distribution<size_t> purpose_dist(0, 63);
    
    // Insert data
    for (size_t i = 0; i < num_keys; ++i) {
        std::string key = "key_" + std::to_string(i);
        auto meta = create_test_metadata(owner_dist(gen), {purpose_dist(gen)}, i % 10, base_time);
        manager.on_put(key, meta.serialize());
    }
    
    // Test with different thread counts
    std::vector<size_t> thread_counts = {1, 2, 4, 8, 16};
    const size_t queries_per_thread = 10000;
    
    for (size_t num_threads : thread_counts) {
        Timer timer;
        
        std::vector<std::thread> threads;
        for (size_t t = 0; t < num_threads; ++t) {
            threads.emplace_back([&, t]() {
                std::mt19937 thread_gen(t);
                std::uniform_int_distribution<size_t> thread_owner_dist(0, 999);
                
                for (size_t i = 0; i < queries_per_thread; ++i) {
                    size_t owner = thread_owner_dist(thread_gen);
                    auto results = manager.find_by_owner(owner);
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        double elapsed = timer.elapsed_ms();
        size_t total_queries = num_threads * queries_per_thread;
        
        std::ostringstream name;
        name << "Concurrent queries (" << num_threads << " threads)";
        print_result(name.str(), total_queries, elapsed);
    }
}


// ============================================================================
// Benchmark 5: Update Performance
// ============================================================================

void benchmark_update_performance() {
    std::cout << "\n=== Update Performance ===" << std::endl;
    print_separator();
    
    controller::IndexConfig config;
    config.enable_owner_index = true;
    config.enable_purpose_index = true;
    config.enable_expiration_index = true;
    config.num_shards = 64;
    controller::IndexManager manager(config);
    
    const size_t num_keys = 10000;
    const uint64_t base_time = 1700000000;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> owner_dist(0, 999);
    std::uniform_int_distribution<size_t> purpose_dist(0, 63);
    
    // Insert initial data
    std::vector<std::tuple<size_t, size_t, size_t>> initial_metadata;  // Store for reuse
    for (size_t i = 0; i < num_keys; ++i) {
        std::string key = "key_" + std::to_string(i);
        size_t owner = owner_dist(gen);
        size_t purpose = purpose_dist(gen);
        initial_metadata.push_back({owner, purpose, owner % 10});
        
        auto meta = create_test_metadata(owner, {purpose}, owner % 10, base_time + i);
        manager.on_put(key, meta.serialize());
    }
    
    // Benchmark: Update with metadata change
    Timer timer;
    for (size_t i = 0; i < num_keys; ++i) {
        std::string key = "key_" + std::to_string(i);
        // CHANGED metadata: different owner, purposes, expiration
        size_t new_owner = (std::get<0>(initial_metadata[i]) + 1) % 1000;
        size_t new_purpose = (std::get<1>(initial_metadata[i]) + 1) % 64;
        auto meta = create_test_metadata(new_owner, {new_purpose, (new_purpose + 1) % 64}, 
                                        new_owner % 10, base_time + i + 3600);  // Different expiration
        manager.on_put(key, meta.serialize());
    }
    double elapsed = timer.elapsed_ms();
    
    print_result("Update 10k keys (metadata changed)", num_keys, elapsed);
    std::cout << "  Avg per update: " << std::fixed << std::setprecision(2) 
              << (elapsed * 1000.0 / num_keys) << " μs" << std::endl;
    
    // Benchmark: Update without metadata change (fast path)
    Timer timer2;
    for (size_t i = 0; i < num_keys; ++i) {
        std::string key = "key_" + std::to_string(i);
        // SAME metadata as previous update - should trigger fast path
        size_t owner = (std::get<0>(initial_metadata[i]) + 1) % 1000;
        size_t purpose = (std::get<1>(initial_metadata[i]) + 1) % 64;
        auto meta = create_test_metadata(owner, {purpose, (purpose + 1) % 64}, 
                                        owner % 10, base_time + i + 3600);  // Same expiration
        manager.on_put(key, meta.serialize());
    }
    double elapsed2 = timer2.elapsed_ms();
    
    print_result("Update 10k keys (no metadata change)", num_keys, elapsed2);
    std::cout << "  Avg per update: " << std::fixed << std::setprecision(2) 
              << (elapsed2 * 1000.0 / num_keys) << " μs" << std::endl;
    std::cout << "  Speedup (fast path): " << std::fixed << std::setprecision(1) 
              << (elapsed / elapsed2) << "x" << std::endl;
}

// ============================================================================
// Benchmark 6: Memory Efficiency
// ============================================================================

void benchmark_memory_efficiency() {
    std::cout << "\n=== Memory Efficiency Analysis ===" << std::endl;
    print_separator();
    
    controller::IndexConfig config;
    config.enable_owner_index = true;
    config.enable_purpose_index = true;
    config.enable_expiration_index = true;
    config.num_shards = 64;
    controller::IndexManager manager(config);
    
    const size_t num_keys = 100000;
    const size_t avg_key_size = 50;
    const uint64_t base_time = 1700000000;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> owner_dist(0, 999);
    std::uniform_int_distribution<size_t> purpose_dist(0, 63);
    
    // Insert data
    for (size_t i = 0; i < num_keys; ++i) {
        std::string key = "user_profile_key_" + std::to_string(i) + "_with_extra_padding";
        auto meta = create_test_metadata(owner_dist(gen), {purpose_dist(gen)}, i % 10, base_time);
        manager.on_put(key, meta.serialize());
    }
    
    std::cout << "\nMemory estimates for " << num_keys << " keys:" << std::endl;
    std::cout << "  Avg key size: " << avg_key_size << " bytes" << std::endl;
    
    // String storage
    size_t string_memory = num_keys * avg_key_size;
    std::cout << "\nString storage (interned):" << std::endl;
    std::cout << "  Total: " << (string_memory / 1024) << " KB" << std::endl;
    
    // Index memory (pointers only)
    size_t num_indexes = 3;
    size_t pointer_memory = num_keys * sizeof(void*) * num_indexes;
    std::cout << "\nIndex storage (pointers only):" << std::endl;
    std::cout << "  Per index: " << (num_keys * sizeof(void*) / 1024) << " KB" << std::endl;
    std::cout << "  Total (" << num_indexes << " indexes): " << (pointer_memory / 1024) << " KB" << std::endl;
    
    size_t total_memory = string_memory + pointer_memory;
    std::cout << "\nTotal estimated memory:" << std::endl;
    std::cout << "  With string interning: " << (total_memory / 1024) << " KB" << std::endl;
    
    size_t without_interning = string_memory * (num_indexes + 1);
    std::cout << "  Without interning: " << (without_interning / 1024) << " KB" << std::endl;
    
    double savings = 100.0 * (1.0 - (double)total_memory / without_interning);
    std::cout << "  Memory savings: " << std::fixed << std::setprecision(1) << savings << "%" << std::endl;
}

// ============================================================================
// Benchmark 7: Range Query Performance (B+ Tree Focus)
// ============================================================================

void benchmark_range_query_performance() {
    std::cout << "\n=== Range Query Performance (B+ Tree) ===" << std::endl;
    print_separator();
    
    controller::IndexConfig config;
    config.enable_expiration_index = true;
    config.num_shards = 64;
    controller::IndexManager manager(config);
    
    const size_t num_keys = 100000;
    const uint64_t base_time = 1700000000;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint64_t> time_dist(0, 365 * 86400);
    std::uniform_int_distribution<size_t> owner_dist(0, 999);
    std::uniform_int_distribution<size_t> purpose_dist(0, 63);
    
    std::cout << "Inserting 100k keys with random expiration times..." << std::flush;
    
    // Insert data with random expiration times
    for (size_t i = 0; i < num_keys; ++i) {
        std::string key = "key_" + std::to_string(i);
        uint64_t exp_time = base_time + time_dist(gen);
        auto meta = create_test_metadata(owner_dist(gen), {purpose_dist(gen)}, 0, exp_time);
        manager.on_put(key, meta.serialize());
        
        if ((i + 1) % 10000 == 0) {
            std::cout << "." << std::flush;
        }
    }
    std::cout << " Done!" << std::endl;
    
    // Test different range sizes
    std::vector<std::pair<std::string, uint64_t>> ranges = {
        {"1 hour", 3600},
        {"1 day", 86400},
        {"1 week", 7 * 86400},
        {"1 month", 30 * 86400},
        {"3 months", 90 * 86400}
    };
    
    const size_t num_queries = 1000;
    
    for (const auto& [name, range_seconds] : ranges) {
        Timer timer;
        size_t total_results = 0;
        
        for (size_t i = 0; i < num_queries; ++i) {
            uint64_t threshold = base_time + range_seconds;
            auto results = manager.find_by_expiration_range(0, threshold);
            total_results += results.size();
        }
        
        double elapsed = timer.elapsed_ms();
        std::ostringstream query_name;
        query_name << "Range query (" << name << ")";
        print_result(query_name.str(), num_queries, elapsed);
        std::cout << "  Avg results: " << (total_results / num_queries) << " keys" << std::endl;
        std::cout << "  Avg latency: " << std::fixed << std::setprecision(2) 
                  << (elapsed * 1000.0 / num_queries) << " μs" << std::endl;
    }
}

// ============================================================================
// Main Benchmark Runner
// ============================================================================

int main() {
    std::cout << "\n";
    print_separator();
    std::cout << "GDPR Index Manager Benchmark Suite" << std::endl;
    print_separator();
    std::cout << "Configuration:" << std::endl;
    std::cout << "  - Users: " << controller::num_users << std::endl;
    std::cout << "  - Purposes: " << controller::num_purposes << std::endl;
    std::cout << "  - Origins: " << controller::num_origins << std::endl;
    std::cout << "  - Shards: 64" << std::endl;
    print_separator();
    
    benchmark_insert_performance();
    benchmark_query_performance();
    benchmark_combined_queries();
    benchmark_concurrent_queries();
    benchmark_update_performance();
    benchmark_memory_efficiency();
    benchmark_range_query_performance();
    
    std::cout << "\n";
    print_separator();
    std::cout << "✓ All benchmarks completed!" << std::endl;
    print_separator();
    std::cout << std::endl;
    
    return 0;
}
