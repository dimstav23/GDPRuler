#include "BenchmarkUtils.hpp"
#include "Compression.hpp"
#include "LogEntry.hpp"
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <vector>

struct Result
{
    int level;
    size_t uncompressedSize;
    size_t compressedSize;
    double compressionRatio;
    long long durationMs;
};

int main()
{
    constexpr size_t batchSize = 1000;
    const std::vector<int> compressionLevels = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    std::vector<Result> results;

    for (int level : compressionLevels)
    {
        // Generate one batch with batchSize entries, no specific destinations
        std::vector<BatchWithDestination> batches = generateBatches(batchSize, 0, batchSize, 4096);
        std::vector<LogEntry> entries = std::move(batches[0].first);

        // Serialize the entries
        std::vector<uint8_t> serializedEntries = LogEntry::serializeBatch(std::move(entries));
        size_t uncompressedSize = serializedEntries.size();

        // Measure compression time
        auto start = std::chrono::high_resolution_clock::now();
        std::vector<uint8_t> compressed = Compression::compress(std::move(serializedEntries), level);
        auto end = std::chrono::high_resolution_clock::now();

        size_t compressedSize = compressed.size();
        double compressionRatio = static_cast<double>(uncompressedSize) / compressedSize;
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        results.push_back({level, uncompressedSize, compressedSize, compressionRatio, duration});
    }

    // Print results
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Level | Uncompressed (B) | Compressed (B) | Ratio | Time (ms)\n";
    std::cout << "------|------------------|----------------|-------|----------\n";
    for (const auto &r : results)
    {
        std::cout << std::setw(5) << r.level << " | "
                  << std::setw(16) << r.uncompressedSize << " | "
                  << std::setw(14) << r.compressedSize << " | "
                  << std::setw(5) << r.compressionRatio << " | "
                  << std::setw(9) << r.durationMs << "\n";
    }

    return 0;
}