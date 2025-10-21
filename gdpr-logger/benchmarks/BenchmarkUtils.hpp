#ifndef BENCHMARK_UTILS_HPP
#define BENCHMARK_UTILS_HPP

#include "LoggingManager.hpp"
#include <vector>
#include <string>
#include <optional>
#include <filesystem>
#include <iostream>
#include <chrono>
#include <algorithm>
#include <random>
#include <numeric>

using BatchWithDestination = std::pair<std::vector<LogEntry>, std::optional<std::string>>;

class LatencyCollector
{
private:
    std::vector<std::chrono::nanoseconds> latencies;

public:
    void addMeasurement(std::chrono::nanoseconds latency)
    {
        latencies.push_back(latency);
    }

    void reserve(size_t capacity)
    {
        latencies.reserve(capacity);
    }

    const std::vector<std::chrono::nanoseconds> &getMeasurements() const
    {
        return latencies;
    }

    void clear()
    {
        latencies.clear();
    }

    // Merge another collector's measurements into this one
    void merge(const LatencyCollector &other)
    {
        const auto &otherLatencies = other.getMeasurements();
        latencies.insert(latencies.end(), otherLatencies.begin(), otherLatencies.end());
    }
};

struct LatencyStats
{
    double maxMs;
    double avgMs;
    double medianMs;
    size_t count;
};

// Function to calculate statistics from a merged collector
LatencyStats calculateLatencyStats(const LatencyCollector &collector);

// Modified to return latency measurements instead of using global state
LatencyCollector appendLogEntries(LoggingManager &loggingManager, const std::vector<BatchWithDestination> &batches);
LatencyCollector appendGDPREntriesIndividually(LoggingManager &loggingManager, 
                                              const std::vector<std::pair<LogEntry, std::string>>& entries, 
                                              int startIndex, int numEntries);

void cleanupLogDirectory(const std::string &logDir);

size_t calculateTotalDataSize(const std::vector<BatchWithDestination> &batches, int numProducers);

size_t calculateDirectorySize(const std::string &dirPath);

std::vector<BatchWithDestination> generateBatches(
    int numEntries,
    int numSpecificFiles,
    int batchSize,
    int payloadSize);

void printLatencyStats(const LatencyStats &stats);

#endif