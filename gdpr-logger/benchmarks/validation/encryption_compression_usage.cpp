#include "BenchmarkUtils.hpp"
#include "LoggingManager.hpp"
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <vector>
#include <future>
#include <optional>
#include <iomanip>
#include <filesystem>

struct BenchmarkResult
{
    bool useEncryption;
    int compressionLevel;
    double executionTime;
    size_t totalEntries;
    double throughputEntries;
    size_t totalDataSizeBytes;
    size_t finalStorageSize;
    double logicalThroughputGiB;
    double physicalThroughputGiB;
    double writeAmplification;
    LatencyStats latencyStats;
};

BenchmarkResult runBenchmark(const LoggingConfig &baseConfig, bool useEncryption, int compressionLevel,
                             const std::vector<BatchWithDestination> &batches,
                             int numProducerThreads, int entriesPerProducer)
{
    LoggingConfig config = baseConfig;
    config.basePath = "./encryption_compression_usage";
    config.useEncryption = useEncryption;
    config.compressionLevel = compressionLevel;

    cleanupLogDirectory(config.basePath);

    size_t totalDataSizeBytes = calculateTotalDataSize(batches, numProducerThreads);
    double totalDataSizeGiB = static_cast<double>(totalDataSizeBytes) / (1024 * 1024 * 1024);
    std::cout << "Benchmark with Encryption: " << (useEncryption ? "Enabled" : "Disabled")
              << ", Compression: " << (compressionLevel != 0 ? "Enabled" : "Disabled")
              << " - Total data to be written: " << totalDataSizeBytes
              << " bytes (" << totalDataSizeGiB << " GiB)" << std::endl;

    LoggingManager loggingManager(config);
    loggingManager.start();
    auto startTime = std::chrono::high_resolution_clock::now();

    // Each future now returns a LatencyCollector with thread-local measurements
    std::vector<std::future<LatencyCollector>> futures;
    for (int i = 0; i < numProducerThreads; i++)
    {
        futures.push_back(std::async(
            std::launch::async,
            appendLogEntries,
            std::ref(loggingManager),
            std::ref(batches)));
    }

    // Collect latency measurements from all threads
    LatencyCollector masterCollector;
    for (auto &future : futures)
    {
        LatencyCollector threadCollector = future.get();
        masterCollector.merge(threadCollector);
    }

    loggingManager.stop();
    auto endTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = endTime - startTime;

    size_t finalStorageSize = calculateDirectorySize(config.basePath);
    double writeAmplification = static_cast<double>(finalStorageSize) / totalDataSizeBytes;

    double elapsedSeconds = elapsed.count();
    const size_t totalEntries = numProducerThreads * entriesPerProducer;
    double throughputEntries = totalEntries / elapsedSeconds;
    double logicalThroughputGiB = totalDataSizeGiB / elapsedSeconds;
    double physicalThroughputGiB = static_cast<double>(finalStorageSize) / (1024.0 * 1024.0 * 1024.0 * elapsedSeconds);

    // Calculate latency statistics from merged measurements
    LatencyStats latencyStats = calculateLatencyStats(masterCollector);

    cleanupLogDirectory(config.basePath);

    return BenchmarkResult{
        useEncryption,
        compressionLevel,
        elapsedSeconds,
        totalEntries,
        throughputEntries,
        totalDataSizeBytes,
        finalStorageSize,
        logicalThroughputGiB,
        physicalThroughputGiB,
        writeAmplification,
        latencyStats};
}

// Write CSV header
void writeCSVHeader(std::ofstream &csvFile)
{
    csvFile << "encryption_enabled,compression_level,execution_time_seconds,total_entries,"
            << "throughput_entries_per_sec,total_data_size_bytes,final_storage_size_bytes,logical_throughput_gib_per_sec,"
            << "physical_throughput_gib_per_sec,write_amplification,avg_latency_ms,median_latency_ms,"
            << "max_latency_ms,latency_count\n";
}

// Write a single result row to CSV
void writeCSVRow(std::ofstream &csvFile, const BenchmarkResult &result)
{
    csvFile << (result.useEncryption ? "true" : "false") << ","
            << result.compressionLevel << ","
            << std::fixed << std::setprecision(6) << result.executionTime << ","
            << result.totalEntries << ","
            << std::fixed << std::setprecision(2) << result.throughputEntries << ","
            << result.totalDataSizeBytes << ","
            << result.finalStorageSize << ","
            << std::fixed << std::setprecision(6) << result.logicalThroughputGiB << ","
            << std::fixed << std::setprecision(6) << result.physicalThroughputGiB << ","
            << std::fixed << std::setprecision(8) << result.writeAmplification << ","
            << std::fixed << std::setprecision(6) << result.latencyStats.avgMs << ","
            << std::fixed << std::setprecision(6) << result.latencyStats.medianMs << ","
            << std::fixed << std::setprecision(6) << result.latencyStats.maxMs << ","
            << result.latencyStats.count << "\n";
}

void runEncryptionCompressionBenchmark(const LoggingConfig &baseConfig,
                                       const std::vector<bool> &encryptionSettings,
                                       const std::vector<int> &compressionLevels,
                                       const std::vector<BatchWithDestination> &batches,
                                       int numProducers, int entriesPerProducer,
                                       const std::string &csvFilename = "encryption_compression_benchmark.csv")
{
    std::vector<BenchmarkResult> results;

    // Open CSV file for writing
    std::ofstream csvFile(csvFilename);
    if (!csvFile.is_open())
    {
        std::cerr << "Error: Could not open CSV file " << csvFilename << " for writing." << std::endl;
        return;
    }

    writeCSVHeader(csvFile);

    int totalCombinations = encryptionSettings.size() * compressionLevels.size();
    std::cout << "Running encryption/compression benchmark with " << totalCombinations << " configurations..." << std::endl;
    std::cout << "Results will be saved to: " << csvFilename << std::endl;

    int currentTest = 0;
    for (bool useEncryption : encryptionSettings)
    {
        for (int compressionLevel : compressionLevels)
        {
            currentTest++;
            std::cout << "\nProgress: " << currentTest << "/" << totalCombinations
                      << " - Testing Encryption: " << (useEncryption ? "Enabled" : "Disabled")
                      << ", Compression: " << compressionLevel << "..." << std::endl;

            BenchmarkResult result = runBenchmark(baseConfig, useEncryption, compressionLevel, batches, numProducers, entriesPerProducer);
            results.push_back(result);

            // Write result to CSV immediately
            writeCSVRow(csvFile, result);
            csvFile.flush(); // Ensure data is written in case of early termination

            // Print progress summary
            std::cout << "  Completed: " << std::fixed << std::setprecision(2)
                      << result.throughputEntries << " entries/s, "
                      << std::fixed << std::setprecision(3) << result.logicalThroughputGiB << " GiB/s, "
                      << "write amp: " << std::fixed << std::setprecision(3) << result.writeAmplification << std::endl;
        }
    }

    csvFile.close();
    std::cout << "\nBenchmark completed! Results saved to " << csvFilename << std::endl;

    // Still print summary table to console for immediate review
    std::cout << "\n============== ENCRYPTION/COMPRESSION LEVEL BENCHMARK SUMMARY ==============" << std::endl;
    std::cout << std::left << std::setw(12) << "Encryption"
              << std::setw(15) << "Comp. Level"
              << std::setw(15) << "Exec. Time (s)"
              << std::setw(20) << "Input Size (bytes)"
              << std::setw(20) << "Storage Size (bytes)"
              << std::setw(12) << "Write Amp."
              << std::setw(20) << "Throughput (ent/s)"
              << std::setw(15) << "Logical (GiB/s)"
              << std::setw(15) << "Physical (GiB/s)"
              << std::setw(12) << "Avg Lat(ms)" << std::endl;
    std::cout << "--------------------------------------------------------------------------------------------------------------------------------" << std::endl;

    // Display results for each configuration
    for (const auto &result : results)
    {
        std::cout << std::left << std::setw(12) << (result.useEncryption ? "True" : "False")
                  << std::setw(15) << result.compressionLevel
                  << std::fixed << std::setprecision(2) << std::setw(15) << result.executionTime
                  << std::setw(20) << result.totalDataSizeBytes
                  << std::setw(20) << result.finalStorageSize
                  << std::fixed << std::setprecision(3) << std::setw(12) << result.writeAmplification
                  << std::fixed << std::setprecision(2) << std::setw(20) << result.throughputEntries
                  << std::fixed << std::setprecision(3) << std::setw(15) << result.logicalThroughputGiB
                  << std::fixed << std::setprecision(3) << std::setw(15) << result.physicalThroughputGiB
                  << std::fixed << std::setprecision(3) << std::setw(12) << result.latencyStats.avgMs << std::endl;
    }

    std::cout << "================================================================================================================================" << std::endl;
}

int main()
{
    // system parameters
    LoggingConfig baseConfig;
    baseConfig.baseFilename = "default";
    baseConfig.maxSegmentSize = 50 * 1024 * 1024; // 50 MB
    baseConfig.maxAttempts = 10;
    baseConfig.baseRetryDelay = std::chrono::milliseconds(2);
    baseConfig.queueCapacity = 3000000;
    baseConfig.maxExplicitProducers = 96;
    baseConfig.batchSize = 8192;
    baseConfig.numWriterThreads = 64;
    baseConfig.appendTimeout = std::chrono::minutes(2);
    // Benchmark parameters
    const int numSpecificFiles = 256;
    const int producerBatchSize = 512;
    const int numProducers = 96;
    const int entriesPerProducer = 260000;
    const int payloadSize = 4096;

    const std::vector<int> compressionLevels = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    const std::vector<bool> encryptionSettings = {false, true};

    std::cout << "Generating batches with pre-determined destinations for all threads...";
    std::vector<BatchWithDestination> batches = generateBatches(entriesPerProducer, numSpecificFiles, producerBatchSize, payloadSize);
    std::cout << " Done." << std::endl;

    runEncryptionCompressionBenchmark(baseConfig,
                                      encryptionSettings,
                                      compressionLevels,
                                      batches,
                                      numProducers,
                                      entriesPerProducer,
                                      "encryption_compression_benchmark_results.csv");

    return 0;
}