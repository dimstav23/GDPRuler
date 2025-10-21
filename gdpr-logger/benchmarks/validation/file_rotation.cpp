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
    int fileCount;
    double writeAmplification;
    LatencyStats latencyStats;
};

int countLogFiles(const std::string &basePath)
{
    int count = 0;
    for (const auto &entry : std::filesystem::directory_iterator(basePath))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".log")
        {
            count++;
        }
    }
    return count;
}

BenchmarkResult runFileRotationBenchmark(
    const LoggingConfig &baseConfig,
    int maxSegmentSizeMB,
    int numProducerThreads,
    int entriesPerProducer,
    int numSpecificFiles,
    int producerBatchSize,
    int payloadSize)
{
    std::string logDir = "./logs/rotation_" + std::to_string(maxSegmentSizeMB) + "mb";

    cleanupLogDirectory(logDir);

    LoggingConfig config = baseConfig;
    config.basePath = logDir;
    config.maxSegmentSize = static_cast<size_t>(maxSegmentSizeMB) * 1024 * 1024;
    std::cout << "Configured max segment size: " << config.maxSegmentSize << " bytes" << std::endl;

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

    size_t finalStorageSize = calculateDirectorySize(logDir);
    double writeAmplification = static_cast<double>(finalStorageSize) / totalDataSizeBytes;

    double elapsedSeconds = elapsed.count();
    const size_t totalEntries = numProducerThreads * entriesPerProducer;
    double throughputEntries = totalEntries / elapsedSeconds;
    double logicalThroughputGiB = totalDataSizeGiB / elapsedSeconds;
    double physicalThroughputGiB = static_cast<double>(finalStorageSize) / (1024.0 * 1024.0 * 1024.0 * elapsedSeconds);
    int fileCount = countLogFiles(logDir);

    // Calculate latency statistics from merged measurements
    LatencyStats latencyStats = calculateLatencyStats(masterCollector);

    cleanupLogDirectory(logDir);

    return BenchmarkResult{
        elapsedSeconds,
        throughputEntries,
        logicalThroughputGiB,
        physicalThroughputGiB,
        fileCount,
        writeAmplification,
        latencyStats};
}

// Write CSV header
void writeCSVHeader(std::ofstream &csvFile)
{
    csvFile << "segment_size_mb,elapsed_seconds,throughput_entries_per_sec,logical_throughput_gib_per_sec,"
            << "physical_throughput_gib_per_sec,file_count,relative_performance,write_amplification,"
            << "avg_latency_ms,median_latency_ms,max_latency_ms,latency_count\n";
}

// Write a single result row to CSV
void writeCSVRow(std::ofstream &csvFile, int segmentSizeMB, const BenchmarkResult &result, double relativePerf)
{
    csvFile << segmentSizeMB << ","
            << std::fixed << std::setprecision(6) << result.elapsedSeconds << ","
            << std::fixed << std::setprecision(2) << result.throughputEntries << ","
            << std::fixed << std::setprecision(6) << result.logicalThroughputGiB << ","
            << std::fixed << std::setprecision(6) << result.physicalThroughputGiB << ","
            << result.fileCount << ","
            << std::fixed << std::setprecision(6) << relativePerf << ","
            << std::fixed << std::setprecision(8) << result.writeAmplification << ","
            << std::fixed << std::setprecision(6) << result.latencyStats.avgMs << ","
            << std::fixed << std::setprecision(6) << result.latencyStats.medianMs << ","
            << std::fixed << std::setprecision(6) << result.latencyStats.maxMs << ","
            << result.latencyStats.count << "\n";
}

void runFileRotationComparison(
    const LoggingConfig &baseConfig,
    const std::vector<int> &segmentSizesMB,
    int numProducerThreads,
    int entriesPerProducer,
    int numSpecificFiles,
    int producerBatchSize,
    int payloadSize,
    const std::string &csvFilename = "file_rotation_benchmark.csv")
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

    std::cout << "Running file rotation benchmark with " << segmentSizesMB.size() << " data points..." << std::endl;
    std::cout << "Results will be saved to: " << csvFilename << std::endl;

    for (size_t i = 0; i < segmentSizesMB.size(); i++)
    {
        int segmentSize = segmentSizesMB[i];
        std::cout << "\nProgress: " << (i + 1) << "/" << segmentSizesMB.size()
                  << " - Running benchmark with segment size: " << segmentSize << " MB..." << std::endl;

        BenchmarkResult result = runFileRotationBenchmark(
            baseConfig,
            segmentSize,
            numProducerThreads,
            entriesPerProducer,
            numSpecificFiles,
            producerBatchSize,
            payloadSize);

        results.push_back(result);

        // Calculate relative performance (using first result as baseline)
        double relativePerf = results.size() > 1 ? result.throughputEntries / results[0].throughputEntries : 1.0;

        // Write result to CSV immediately
        writeCSVRow(csvFile, segmentSize, result, relativePerf);
        csvFile.flush(); // Ensure data is written in case of early termination

        // Print progress summary
        std::cout << "  Completed: " << std::fixed << std::setprecision(2)
                  << result.throughputEntries << " entries/s, "
                  << std::fixed << std::setprecision(3) << result.logicalThroughputGiB << " GiB/s, "
                  << result.fileCount << " files created" << std::endl;

        // Add a small delay between runs
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    csvFile.close();
    std::cout << "\nBenchmark completed! Results saved to " << csvFilename << std::endl;

    // Still print summary table to console for immediate review
    std::cout << "\n========================== FILE ROTATION BENCHMARK SUMMARY ==========================" << std::endl;
    std::cout << std::left << std::setw(20) << "Segment Size (MB)"
              << std::setw(15) << "Time (sec)"
              << std::setw(20) << "Throughput (ent/s)"
              << std::setw(15) << "Logical (GiB/s)"
              << std::setw(15) << "Physical (GiB/s)"
              << std::setw(15) << "Files Created"
              << std::setw(15) << "Write Amp."
              << std::setw(12) << "Rel. Perf"
              << std::setw(12) << "Avg Lat(ms)" << std::endl;
    std::cout << "--------------------------------------------------------------------------------------------------------------------------------" << std::endl;

    // Use the first segment size as the baseline for relative performance
    double baselineThroughput = results[0].throughputEntries;

    for (size_t i = 0; i < segmentSizesMB.size(); i++)
    {
        double relativePerf = results[i].throughputEntries / baselineThroughput;
        std::cout << std::left << std::setw(20) << segmentSizesMB[i]
                  << std::setw(15) << std::fixed << std::setprecision(2) << results[i].elapsedSeconds
                  << std::setw(20) << std::fixed << std::setprecision(2) << results[i].throughputEntries
                  << std::setw(15) << std::fixed << std::setprecision(3) << results[i].logicalThroughputGiB
                  << std::setw(15) << std::fixed << std::setprecision(3) << results[i].physicalThroughputGiB
                  << std::setw(15) << results[i].fileCount
                  << std::setw(15) << std::fixed << std::setprecision(4) << results[i].writeAmplification
                  << std::setw(12) << std::fixed << std::setprecision(2) << relativePerf
                  << std::setw(12) << std::fixed << std::setprecision(3) << results[i].latencyStats.avgMs << std::endl;
    }
    std::cout << "================================================================================================================================" << std::endl;
}

int main()
{
    // system parameters
    LoggingConfig baseConfig;
    baseConfig.baseFilename = "default";
    baseConfig.maxAttempts = 5;
    baseConfig.baseRetryDelay = std::chrono::milliseconds(1);
    baseConfig.queueCapacity = 3000000;
    baseConfig.maxExplicitProducers = 32;
    baseConfig.batchSize = 8192;
    baseConfig.numWriterThreads = 64;
    baseConfig.appendTimeout = std::chrono::minutes(2);
    baseConfig.useEncryption = false;
    baseConfig.compressionLevel = 0;
    // benchmark parameters
    const int numSpecificFiles = 0;
    const int producerBatchSize = 1024;
    const int numProducers = 32;
    const int entriesPerProducer = 1000000;
    const int payloadSize = 256;

    std::vector<int> segmentSizesMB = {8000, 6000, 4000, 3000, 2000, 1500, 1000, 800, 650, 500, 350, 250, 150, 100, 85, 70, 55, 40, 25, 10};

    runFileRotationComparison(
        baseConfig,
        segmentSizesMB,
        numProducers,
        entriesPerProducer,
        numSpecificFiles,
        producerBatchSize,
        payloadSize,
        "file_rotation_benchmark_results.csv");

    return 0;
}