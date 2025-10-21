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

BenchmarkResult runFilepathDiversityBenchmark(const LoggingConfig &config, int numSpecificFiles, int numProducerThreads,
                                              int entriesPerProducer, int producerBatchSize, int payloadSize)
{
    LoggingConfig runConfig = config;
    runConfig.basePath = "./logs/files_" + std::to_string(numSpecificFiles);

    cleanupLogDirectory(runConfig.basePath);

    std::cout << "Generating batches with " << numSpecificFiles << " specific files for all threads...";
    std::vector<BatchWithDestination> batches = generateBatches(entriesPerProducer, numSpecificFiles, producerBatchSize, payloadSize);
    std::cout << " Done." << std::endl;
    size_t totalDataSizeBytes = calculateTotalDataSize(batches, numProducerThreads);
    double totalDataSizeGiB = static_cast<double>(totalDataSizeBytes) / (1024 * 1024 * 1024);
    std::cout << "Total data to be written: " << totalDataSizeBytes << " bytes ("
              << totalDataSizeGiB << " GiB)" << std::endl;

    LoggingManager loggingManager(runConfig);
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

    size_t finalStorageSize = calculateDirectorySize(runConfig.basePath);
    double writeAmplification = static_cast<double>(finalStorageSize) / totalDataSizeBytes;

    double elapsedSeconds = elapsed.count();
    const size_t totalEntries = numProducerThreads * entriesPerProducer;
    double throughputEntries = totalEntries / elapsedSeconds;
    double logicalThroughputGiB = totalDataSizeGiB / elapsedSeconds;
    double physicalThroughputGiB = static_cast<double>(finalStorageSize) / (1024.0 * 1024.0 * 1024.0 * elapsedSeconds);

    // Calculate latency statistics from merged measurements
    LatencyStats latencyStats = calculateLatencyStats(masterCollector);

    cleanupLogDirectory(runConfig.basePath);

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
    csvFile << "num_specific_files,configuration_description,elapsed_seconds,throughput_entries_per_sec,logical_throughput_gib_per_sec,"
            << "physical_throughput_gib_per_sec,relative_performance,write_amplification,"
            << "avg_latency_ms,median_latency_ms,max_latency_ms,latency_count\n";
}

// Write a single result row to CSV
void writeCSVRow(std::ofstream &csvFile, int numSpecificFiles, const std::string &description, const BenchmarkResult &result, double relativePerf)
{
    csvFile << numSpecificFiles << ","
            << "\"" << description << "\"," // Quote the description in case it contains commas
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

void runFilepathDiversityComparison(const LoggingConfig &config, const std::vector<int> &numFilesVariants,
                                    int numProducerThreads, int entriesPerProducer, int producerBatchSize, int payloadSize,
                                    const std::string &csvFilename = "diverse_filepaths_benchmark.csv")
{
    std::vector<BenchmarkResult> results;
    std::vector<std::string> descriptions;

    // Generate descriptions for each file count variant
    for (int fileCount : numFilesVariants)
    {
        if (fileCount == 0)
        {
            descriptions.push_back("Default file only");
        }
        else if (fileCount == 1)
        {
            descriptions.push_back("1 specific file");
        }
        else
        {
            descriptions.push_back(std::to_string(fileCount) + " specific files");
        }
    }

    // Open CSV file for writing
    std::ofstream csvFile(csvFilename);
    if (!csvFile.is_open())
    {
        std::cerr << "Error: Could not open CSV file " << csvFilename << " for writing." << std::endl;
        return;
    }

    writeCSVHeader(csvFile);

    std::cout << "Running filepath diversity benchmark with " << numFilesVariants.size() << " data points..." << std::endl;
    std::cout << "Results will be saved to: " << csvFilename << std::endl;

    for (size_t i = 0; i < numFilesVariants.size(); i++)
    {
        int fileCount = numFilesVariants[i];
        std::cout << "\nProgress: " << (i + 1) << "/" << numFilesVariants.size()
                  << " - Running benchmark with " << descriptions[i] << "..." << std::endl;

        BenchmarkResult result = runFilepathDiversityBenchmark(
            config,
            fileCount,
            numProducerThreads, entriesPerProducer, producerBatchSize, payloadSize);

        results.push_back(result);

        // Calculate relative performance (using first result as baseline)
        double relativePerf = results.size() > 1 ? result.throughputEntries / results[0].throughputEntries : 1.0;

        // Write result to CSV immediately
        writeCSVRow(csvFile, fileCount, descriptions[i], result, relativePerf);
        csvFile.flush(); // Ensure data is written in case of early termination

        // Print progress summary
        std::cout << "  Completed: " << std::fixed << std::setprecision(2)
                  << result.throughputEntries << " entries/s, "
                  << std::fixed << std::setprecision(3) << result.logicalThroughputGiB << " GiB/s, "
                  << std::fixed << std::setprecision(2) << relativePerf << "x relative performance" << std::endl;

        // Add a small delay between runs
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    csvFile.close();
    std::cout << "\nBenchmark completed! Results saved to " << csvFilename << std::endl;

    // Still print summary table to console for immediate review
    std::cout << "\n=========== FILEPATH DIVERSITY BENCHMARK SUMMARY ===========" << std::endl;
    std::cout << std::left << std::setw(25) << "Configuration"
              << std::setw(15) << "Time (sec)"
              << std::setw(20) << "Throughput (ent/s)"
              << std::setw(15) << "Logical (GiB/s)"
              << std::setw(15) << "Physical (GiB/s)"
              << std::setw(15) << "Write Amp."
              << std::setw(12) << "Rel. Perf"
              << std::setw(12) << "Avg Lat(ms)" << std::endl;
    std::cout << "--------------------------------------------------------------------------------------------------------------------------------" << std::endl;

    // Calculate base throughput for relative performance
    double baseThroughputEntries = results[0].throughputEntries;

    for (size_t i = 0; i < numFilesVariants.size(); i++)
    {
        double relativePerf = results[i].throughputEntries / baseThroughputEntries;
        std::cout << std::left << std::setw(25) << descriptions[i]
                  << std::setw(15) << std::fixed << std::setprecision(2) << results[i].elapsedSeconds
                  << std::setw(20) << std::fixed << std::setprecision(2) << results[i].throughputEntries
                  << std::setw(15) << std::fixed << std::setprecision(3) << results[i].logicalThroughputGiB
                  << std::setw(15) << std::fixed << std::setprecision(3) << results[i].physicalThroughputGiB
                  << std::setw(15) << std::fixed << std::setprecision(4) << results[i].writeAmplification
                  << std::setw(12) << std::fixed << std::setprecision(2) << relativePerf
                  << std::setw(12) << std::fixed << std::setprecision(3) << results[i].latencyStats.avgMs << std::endl;
    }
    std::cout << "======================================================================================================================================" << std::endl;
}

int main()
{
    // system parameters
    LoggingConfig config;
    config.baseFilename = "default";
    config.maxSegmentSize = static_cast<size_t>(1000) * 1024 * 1024; // 1 GB
    config.maxAttempts = 10;
    config.baseRetryDelay = std::chrono::milliseconds(2);
    config.queueCapacity = 3000000;
    config.maxExplicitProducers = 32;
    config.batchSize = 8192;
    config.numWriterThreads = 64;
    config.appendTimeout = std::chrono::minutes(2);
    config.useEncryption = true;
    config.compressionLevel = 9;
    config.maxOpenFiles = 256;
    // benchmark parameters
    const int producerBatchSize = 8192;
    const int numProducers = 32;
    const int entriesPerProducer = 2000000;
    const int payloadSize = 2048;

    std::vector<int> numFilesVariants = {0, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192};

    runFilepathDiversityComparison(config,
                                   numFilesVariants,
                                   numProducers,
                                   entriesPerProducer,
                                   producerBatchSize,
                                   payloadSize,
                                   "diverse_filepaths_benchmark_results.csv");

    return 0;
}