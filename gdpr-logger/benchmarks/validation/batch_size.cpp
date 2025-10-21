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
    double elapsedSeconds;
    double throughputEntries;
    double logicalThroughputGiB;
    double physicalThroughputGiB;
    double writeAmplification;
    LatencyStats latencyStats;
};

BenchmarkResult runBatchSizeBenchmark(const LoggingConfig &baseConfig, int writerBatchSize, int numProducerThreads,
                                      int entriesPerProducer, int numSpecificFiles, int producerBatchSize, int payloadSize)
{
    LoggingConfig config = baseConfig;
    config.basePath = "./logs/batch_" + std::to_string(writerBatchSize);
    config.batchSize = writerBatchSize;

    cleanupLogDirectory(config.basePath);

    std::cout << "Generating batches with pre-determined destinations for all threads...";
    std::vector<BatchWithDestination> batches = generateBatches(entriesPerProducer, numSpecificFiles, producerBatchSize, payloadSize);
    std::cout << " Done." << std::endl;

    size_t totalDataSizeBytes = calculateTotalDataSize(batches, numProducerThreads);
    double totalDataSizeGiB = static_cast<double>(totalDataSizeBytes) / (1024 * 1024 * 1024);
    std::cout << "Total data to be written: " << totalDataSizeBytes << " bytes ("
              << totalDataSizeGiB << " GiB)" << std::endl;

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
        elapsedSeconds,
        throughputEntries,
        logicalThroughputGiB,
        physicalThroughputGiB,
        writeAmplification,
        latencyStats};
}

// Write CSV header
void writeCSVHeader(std::ofstream &csvFile)
{
    csvFile << "batch_size,elapsed_seconds,throughput_entries_per_sec,logical_throughput_gib_per_sec,"
            << "physical_throughput_gib_per_sec,relative_performance,write_amplification,"
            << "avg_latency_ms,median_latency_ms,max_latency_ms,latency_count\n";
}

// Write a single result row to CSV
void writeCSVRow(std::ofstream &csvFile, int batchSize, const BenchmarkResult &result, double relativePerf)
{
    csvFile << batchSize << ","
            << std::fixed << std::setprecision(6) << result.elapsedSeconds << ","
            << std::fixed << std::setprecision(2) << result.throughputEntries << ","
            << std::fixed << std::setprecision(6) << result.logicalThroughputGiB << ","
            << std::fixed << std::setprecision(6) << result.physicalThroughputGiB << ","
            << std::fixed << std::setprecision(6) << relativePerf << ","
            << std::fixed << std::setprecision(8) << result.writeAmplification << ","
            << std::fixed << std::setprecision(6) << result.latencyStats.avgMs << ","
            << std::fixed << std::setprecision(6) << result.latencyStats.medianMs << ","
            << std::fixed << std::setprecision(6) << result.latencyStats.maxMs << ","
            << result.latencyStats.count << "\n";
}

void runBatchSizeComparison(const LoggingConfig &baseConfig, const std::vector<int> &batchSizes,
                            int numProducerThreads, int entriesPerProducer,
                            int numSpecificFiles, int producerBatchSize, int payloadSize,
                            const std::string &csvFilename = "batch_size_benchmark.csv")
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

    std::cout << "Running batch size benchmark with " << batchSizes.size() << " data points..." << std::endl;
    std::cout << "Results will be saved to: " << csvFilename << std::endl;

    for (size_t i = 0; i < batchSizes.size(); i++)
    {
        int batchSize = batchSizes[i];
        std::cout << "\nProgress: " << (i + 1) << "/" << batchSizes.size()
                  << " - Running benchmark with writer batch size: " << batchSize << "..." << std::endl;

        BenchmarkResult result = runBatchSizeBenchmark(
            baseConfig, batchSize, numProducerThreads,
            entriesPerProducer, numSpecificFiles, producerBatchSize, payloadSize);

        results.push_back(result);

        // Calculate relative performance (using first result as baseline)
        double relativePerf = results.size() > 1 ? result.throughputEntries / results[0].throughputEntries : 1.0;

        // Write result to CSV immediately
        writeCSVRow(csvFile, batchSize, result, relativePerf);
        csvFile.flush(); // Ensure data is written in case of early termination

        // Print progress summary
        std::cout << "  Completed: " << std::fixed << std::setprecision(2)
                  << result.throughputEntries << " entries/s, "
                  << std::fixed << std::setprecision(3) << result.logicalThroughputGiB << " GiB/s" << std::endl;

        // Small delay between runs
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    csvFile.close();
    std::cout << "\nBenchmark completed! Results saved to " << csvFilename << std::endl;

    std::cout << "\n=========== WRITER BATCH SIZE BENCHMARK SUMMARY ===========" << std::endl;
    std::cout << std::left << std::setw(12) << "Batch Size"
              << std::setw(15) << "Time (sec)"
              << std::setw(20) << "Throughput (entries/s)"
              << std::setw(15) << "Logical (GiB/s)"
              << std::setw(15) << "Physical (GiB/s)"
              << std::setw(12) << "Rel. Perf"
              << std::setw(15) << "Write Amp."
              << std::setw(12) << "Avg Lat(ms)" << std::endl;
    std::cout << "--------------------------------------------------------------------------------------------------------------------------------" << std::endl;

    for (size_t i = 0; i < batchSizes.size(); i++)
    {
        double relativePerf = results[i].throughputEntries / results[0].throughputEntries;
        std::cout << std::left << std::setw(12) << batchSizes[i]
                  << std::setw(15) << std::fixed << std::setprecision(2) << results[i].elapsedSeconds
                  << std::setw(20) << std::fixed << std::setprecision(2) << results[i].throughputEntries
                  << std::setw(15) << std::fixed << std::setprecision(3) << results[i].logicalThroughputGiB
                  << std::setw(15) << std::fixed << std::setprecision(3) << results[i].physicalThroughputGiB
                  << std::setw(12) << std::fixed << std::setprecision(2) << relativePerf
                  << std::setw(15) << std::fixed << std::setprecision(4) << results[i].writeAmplification
                  << std::setw(12) << std::fixed << std::setprecision(3) << results[i].latencyStats.avgMs << std::endl;
    }
    std::cout << "======================================================================================================================================" << std::endl;
}

int main()
{
    // System parameters
    LoggingConfig baseConfig;
    baseConfig.baseFilename = "default";
    baseConfig.maxSegmentSize = 500 * 1024 * 1024; // 100 MB
    baseConfig.maxAttempts = 5;
    baseConfig.baseRetryDelay = std::chrono::milliseconds(1);
    baseConfig.queueCapacity = 2000000;
    baseConfig.maxExplicitProducers = 16;
    baseConfig.numWriterThreads = 16;
    baseConfig.appendTimeout = std::chrono::minutes(2);
    baseConfig.useEncryption = true;
    baseConfig.compressionLevel = 4;
    baseConfig.maxOpenFiles = 512;
    // Benchmark parameters
    const int numSpecificFiles = 256;
    const int producerBatchSize = 4096;
    const int numProducers = 16;
    const int entriesPerProducer = 2000000;
    const int payloadSize = 4096;

    std::vector<int> batchSizes = {1, 4, 8, 16, 32, 64, 96, 128, 256, 512, 768, 1024, 1536, 2048, 4096, 8192, 16384, 32768, 65536, 131072};

    runBatchSizeComparison(baseConfig,
                           batchSizes,
                           numProducers,
                           entriesPerProducer,
                           numSpecificFiles,
                           producerBatchSize,
                           payloadSize,
                           "batch_size_benchmark_results.csv");

    return 0;
}