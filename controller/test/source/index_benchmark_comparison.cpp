#include <iostream>
#include <chrono>
#include <iomanip>
#include "gdpr_index/index_manager.hpp"
#include "gdpr_metadata.hpp"

// Reuse helper functions from main benchmark
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
    size_t origin_bit = 0;
    uint64_t expiration_time = 0;
    std::string actual_data;
    
    std::string serialize() const {
        std::string result;
        
        controller::metadata_header header;
        header.user_bytes = (controller::num_users + 7) / 8;
        header.purpose_bytes = (controller::num_purposes + 7) / 8;
        header.origin_bytes = (controller::num_origins + 7) / 8;
        header.flags = 0;
        header.expiration_time = expiration_time;
        
        result.append(reinterpret_cast<const char*>(&header), sizeof(header));
        
        std::bitset<controller::num_users> owner_bitmap;
        owner_bitmap.set(owner_bit);
        result.append(bitset_to_bytes(owner_bitmap));
        
        result.append(bitset_to_bytes(purpose));
        
        std::bitset<controller::num_purposes> objection;
        result.append(bitset_to_bytes(objection));
        
        std::bitset<controller::num_origins> origin_bitmap;
        origin_bitmap.set(origin_bit);
        result.append(bitset_to_bytes(origin_bitmap));
        
        std::bitset<controller::num_users> share;
        result.append(bitset_to_bytes(share));
        result.append(actual_data);
        
        return result;
    }
};

GDPRMetadata create_test_metadata(size_t owner, const std::vector<size_t>& purposes, size_t origin, uint64_t expiration = 0) {
    GDPRMetadata meta;
    meta.owner_bit = owner % controller::num_users;
    meta.origin_bit = origin % controller::num_origins;
    meta.expiration_time = expiration;
    meta.actual_data = "benchmark_data";
    
    for (size_t p : purposes) {
        if (p < controller::num_purposes) {
            meta.purpose.set(p);
        }
    }
    
    return meta;
}

class Timer {
    std::chrono::steady_clock::time_point start_;
public:
    Timer() : start_(std::chrono::steady_clock::now()) {}
    double elapsed_ms() const {
        auto end = std::chrono::steady_clock::now();
        return std::chrono::duration<double, std::milli>(end - start_).count();
    }
};

void benchmark_implementation(controller::BitInvertedIndexType impl_type, const std::string& impl_name) {
    std::cout << "\n==== Testing: " << impl_name << " ====" << std::endl;
    
    controller::IndexConfig config;
    config.enable_purpose_index = true;
    config.purpose_index_impl = impl_type;
    config.num_shards = 64;
    controller::IndexManager manager(config);
    
    const size_t num_keys = 100000;
    const uint64_t base_time = 1700000000;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> owner_dist(0, 999);
    std::uniform_int_distribution<size_t> purpose_dist(0, 63);
    
    // Insert
    std::cout << "Inserting 100k keys... " << std::flush;
    for (size_t i = 0; i < num_keys; ++i) {
        std::string key = "key_" + std::to_string(i);
        size_t purpose1 = purpose_dist(gen);
        size_t purpose2 = (purpose1 + 1) % 64;
        auto meta = create_test_metadata(owner_dist(gen), {purpose1, purpose2}, i % 10, base_time);
        manager.on_put(key, meta.serialize());
    }
    std::cout << "Done!" << std::endl;
    
    const size_t num_queries = 10000;
    
    // Single-bit query
    {
        Timer timer;
        size_t total_results = 0;
        
        for (size_t i = 0; i < num_queries; ++i) {
            std::bitset<controller::num_purposes> purpose_query;
            purpose_query.set(i % 64);
            auto results = manager.find_by_purpose(purpose_query);
            total_results += results.size();
        }
        
        double elapsed = timer.elapsed_ms();
        std::cout << "  1-bit query:  " << std::fixed << std::setprecision(2) << elapsed << " ms "
                  << "(" << (int)(elapsed * 1000.0 / num_queries) << " μs/query, "
                  << (total_results / num_queries) << " avg results)" << std::endl;
    }
    
    // 4-bit query
    {
        Timer timer;
        size_t total_results = 0;
        
        for (size_t i = 0; i < num_queries; ++i) {
            std::bitset<controller::num_purposes> purpose_query;
            purpose_query.set(i % 64);
            purpose_query.set((i + 1) % 64);
            purpose_query.set((i + 2) % 64);
            purpose_query.set((i + 3) % 64);
            auto results = manager.find_by_purpose(purpose_query);
            total_results += results.size();
        }
        
        double elapsed = timer.elapsed_ms();
        std::cout << "  4-bit query:  " << std::fixed << std::setprecision(2) << elapsed << " ms "
                  << "(" << (int)(elapsed * 1000.0 / num_queries) << " μs/query, "
                  << (total_results / num_queries) << " avg results)" << std::endl;
    }
}

int main() {
    std::cout << "\n================================================" << std::endl;
    std::cout << "GDPR Index: HashSet vs Roaring Comparison" << std::endl;
    std::cout << "================================================\n" << std::endl;
    
    // Test HashSet
    benchmark_implementation(controller::BitInvertedIndexType::HASHSET, "HashSet");
    // Test Roaring
    benchmark_implementation(controller::BitInvertedIndexType::ROARING, "Roaring Bitmap");
    
    return 0;
}
