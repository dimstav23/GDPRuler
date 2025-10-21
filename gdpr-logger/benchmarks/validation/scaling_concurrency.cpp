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
    double executionTime;
    double throughputEntries;
    double logicalThroughputGiB;
    double physicalThroughputGiB;
    size_t inputDataSizeBytes;
    size_t outputDataSizeBytes;
    double writeAmplification;
    LatencyStats latencyStats;
};

BenchmarkResult runBenchmark(const LoggingConfig &baseConfig, int numWriterThreads, int numProducerThreads,
                             int entriesPerProducer, int numSpecificFiles, int producerBatchSize, int payloadSize)
{
    LoggingConfig config = baseConfig;
    config.basePath = "./logs_writers";
    config.numWriterThreads = numWriterThreads;
    config.maxExplicitProducers = numProducerThreads;

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
        totalDataSizeBytes,
        finalStorageSize,
        writeAmplification,
        latencyStats};
}

// Write CSV header
void writeCSVHeader(std::ofstream &csvFile)
{
    csvFile << "writer_threads,producer_threads,execution_time_seconds,throughput_entries_per_sec,logical_throughput_gib_per_sec,"
            << "physical_throughput_gib_per_sec,input_data_size_bytes,output_data_size_bytes,scaling_efficiency,"
            << "write_amplification,avg_latency_ms,median_latency_ms,max_latency_ms,latency_count\n";
}

// Write a single result row to CSV
void writeCSVRow(std::ofstream &csvFile, int writerThreads, int producerThreads, const BenchmarkResult &result, double scalingEfficiency)
{
    csvFile << writerThreads << ","
            << producerThreads << ","
            << std::fixed << std::setprecision(6) << result.executionTime << ","
            << std::fixed << std::setprecision(2) << result.throughputEntries << ","
            << std::fixed << std::setprecision(6) << result.logicalThroughputGiB << ","
            << std::fixed << std::setprecision(6) << result.physicalThroughputGiB << ","
            << result.inputDataSizeBytes << ","
            << result.outputDataSizeBytes << ","
            << std::fixed << std::setprecision(6) << scalingEfficiency << ","
            << std::fixed << std::setprecision(8) << result.writeAmplification << ","
            << std::fixed << std::setprecision(6) << result.latencyStats.avgMs << ","
            << std::fixed << std::setprecision(6) << result.latencyStats.medianMs << ","
            << std::fixed << std::setprecision(6) << result.latencyStats.maxMs << ","
            << result.latencyStats.count << "\n";
}

void runScalabilityBenchmark(const LoggingConfig &baseConfig, const std::vector<int> &writerThreadCounts,
                             int baseProducerThreads, int baseEntriesPerProducer,
                             int numSpecificFiles, int producerBatchSize, int payloadSize,
                             const std::string &csvFilename = "scaling_concurrency_benchmark.csv")
{
    std::vector<BenchmarkResult> results;
    std::vector<int> producerThreadCounts;

    // Open CSV file for writing
    std::ofstream csvFile(csvFilename);
    if (!csvFile.is_open())
    {
        std::cerr << "Error: Could not open CSV file " << csvFilename << " for writing." << std::endl;
        return;
    }

    writeCSVHeader(csvFile);

    std::cout << "Running scaling concurrency benchmark with " << writerThreadCounts.size() << " data points..." << std::endl;
    std::cout << "Results will be saved to: " << csvFilename << std::endl;

    for (size_t i = 0; i < writerThreadCounts.size(); i++)
    {
        int writerCount = writerThreadCounts[i];
        std::cout << "\nProgress: " << (i + 1) << "/" << writerThreadCounts.size()
                  << " - Running scalability benchmark with " << writerCount << " writer thread(s)..." << std::endl;

        // Option 1: Scale producer threads, keeping entries per producer constant
        int scaledProducers = baseProducerThreads * writerCount;
        int entriesPerProducer = baseEntriesPerProducer;
        producerThreadCounts.push_back(scaledProducers);

        std::cout << "Scaled workload: " << scaledProducers << " producers, "
                  << entriesPerProducer << " entries per producer" << std::endl;

        BenchmarkResult result = runBenchmark(baseConfig, writerCount, scaledProducers, entriesPerProducer,
                                              numSpecificFiles, producerBatchSize, payloadSize);

        results.push_back(result);

        // Calculate scaling efficiency (normalized by expected linear scaling)
        double scalingEfficiency = results.size() > 1 ? (result.throughputEntries / results[0].throughputEntries) / writerCount : 1.0;

        // Write result to CSV immediately
        writeCSVRow(csvFile, writerCount, scaledProducers, result, scalingEfficiency);
        csvFile.flush(); // Ensure data is written in case of early termination

        // Print progress summary
        std::cout << "  Completed: " << std::fixed << std::setprecision(2)
                  << result.throughputEntries << " entries/s, "
                  << std::fixed << std::setprecision(3) << result.logicalThroughputGiB << " GiB/s, "
                  << std::fixed << std::setprecision(2) << scalingEfficiency << " scaling efficiency" << std::endl;
    }

    csvFile.close();
    std::cout << "\nBenchmark completed! Results saved to " << csvFilename << std::endl;

    // Still print summary table to console for immediate review
    std::cout << "\n=================== SCALABILITY BENCHMARK SUMMARY ===================" << std::endl;
    std::cout << std::left << std::setw(20) << "Writer Threads"
              << std::setw(20) << "Producer Threads"
              << std::setw(15) << "Time (sec)"
              << std::setw(20) << "Throughput (ent/s)"
              << std::setw(15) << "Logical (GiB/s)"
              << std::setw(15) << "Physical (GiB/s)"
              << std::setw(20) << "Input Size (bytes)"
              << std::setw(20) << "Storage Size (bytes)"
              << std::setw(15) << "Write Amp."
              << std::setw(12) << "Rel. Perf."
              << std::setw(12) << "Avg Lat(ms)" << std::endl;
    std::cout << "--------------------------------------------------------------------------------------------------------------------------------" << std::endl;

    double baselineThroughput = results[0].throughputEntries;

    for (size_t i = 0; i < writerThreadCounts.size(); i++)
    {
        double relativePerformance = results[i].throughputEntries / (baselineThroughput * writerThreadCounts[i]);

        std::cout << std::left << std::setw(20) << writerThreadCounts[i]
                  << std::setw(20) << producerThreadCounts[i]
                  << std::setw(15) << std::fixed << std::setprecision(2) << results[i].executionTime
                  << std::setw(20) << std::fixed << std::setprecision(2) << results[i].throughputEntries
                  << std::setw(15) << std::fixed << std::setprecision(3) << results[i].logicalThroughputGiB
                  << std::setw(15) << std::fixed << std::setprecision(3) << results[i].physicalThroughputGiB
                  << std::setw(20) << results[i].inputDataSizeBytes
                  << std::setw(20) << results[i].outputDataSizeBytes
                  << std::setw(15) << std::fixed << std::setprecision(4) << results[i].writeAmplification
                  << std::setw(12) << std::fixed << std::setprecision(2) << relativePerformance
                  << std::setw(12) << std::fixed << std::setprecision(3) << results[i].latencyStats.avgMs << std::endl;
    }
    std::cout << "================================================================================================================================" << std::endl;
}

int main()
{
    // system parameters
    LoggingConfig baseConfig;
    baseConfig.baseFilename = "default";
    baseConfig.maxSegmentSize = 250 * 1024 * 1024; // 250 MB
    baseConfig.maxAttempts = 5;
    baseConfig.baseRetryDelay = std::chrono::milliseconds(1);
    baseConfig.queueCapacity = 3000000;
    baseConfig.batchSize = 8192;
    baseConfig.appendTimeout = std::chrono::minutes(5);
    baseConfig.useEncryption = true;
    baseConfig.compressionLevel = 9;
    // benchmark parameters
    const int numSpecificFiles = 256;
    const int producerBatchSize = 512;
    const int baseProducerThreads = 1;
    const int baseEntriesPerProducer = 4000000;
    const int payloadSize = 2048;

    std::vector<int> writerThreadCounts = {1, 2, 4, 8, 12, 16, 20, 24, 28, 32, 40, 48, 56, 64};

    runScalabilityBenchmark(baseConfig,
                            writerThreadCounts,
                            baseProducerThreads,
                            baseEntriesPerProducer,
                            numSpecificFiles,
                            producerBatchSize,
                            payloadSize,
                            "scaling_concurrency_benchmark_results.csv");

    return 0;
}