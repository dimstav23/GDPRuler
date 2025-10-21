#include "BenchmarkUtils.hpp"

LatencyCollector appendGDPREntriesIndividually(LoggingManager &loggingManager, 
                                              const std::vector<std::pair<LogEntry, std::string>>& entries,
                                              int startIndex, int numEntries) {
    LatencyCollector localCollector;
    localCollector.reserve(numEntries);

    auto token = loggingManager.createProducerToken();

    for (int i = 0; i < numEntries; ++i) {
        const auto& [entry, filename] = entries[startIndex + i];
        
        // Measure latency for each append call
        auto startTime = std::chrono::high_resolution_clock::now();

        bool success = loggingManager.append(entry, token, filename);

        auto endTime = std::chrono::high_resolution_clock::now();
        auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - startTime);

        // Record the latency measurement
        localCollector.addMeasurement(latency);

        if (!success) {
            std::cerr << "Failed to append GDPR entry to " << filename << std::endl;
        }
    }

    return localCollector;
}

LatencyCollector appendLogEntries(LoggingManager &loggingManager, const std::vector<BatchWithDestination> &batches)
{
    LatencyCollector localCollector;
    // Pre-allocate to avoid reallocations during measurement
    localCollector.reserve(batches.size());

    auto token = loggingManager.createProducerToken();

    for (const auto &batchWithDest : batches)
    {
        // Measure latency for each appendBatch call
        auto startTime = std::chrono::high_resolution_clock::now();

        bool success = loggingManager.appendBatch(batchWithDest.first, token, batchWithDest.second);

        auto endTime = std::chrono::high_resolution_clock::now();
        auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - startTime);

        // Record the latency measurement in thread-local collector
        localCollector.addMeasurement(latency);

        if (!success)
        {
            std::cerr << "Failed to append batch of " << batchWithDest.first.size() << " entries to "
                      << (batchWithDest.second ? *batchWithDest.second : "default") << std::endl;
        }
    }

    return localCollector;
}

void cleanupLogDirectory(const std::string &logDir)
{
    try
    {
        if (std::filesystem::exists(logDir))
        {
            std::filesystem::remove_all(logDir);
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error cleaning log directory: " << e.what() << std::endl;
    }
}

size_t calculateTotalDataSize(const std::vector<BatchWithDestination> &batches, int numProducers)
{
    size_t totalSize = 0;

    for (const auto &batchWithDest : batches)
    {
        for (const auto &entry : batchWithDest.first)
        {
            totalSize += entry.serialize().size();
        }
    }

    return totalSize * numProducers;
}

size_t calculateDirectorySize(const std::string &dirPath)
{
    size_t totalSize = 0;
    for (const auto &entry : std::filesystem::recursive_directory_iterator(dirPath))
    {
        if (entry.is_regular_file())
        {
            totalSize += std::filesystem::file_size(entry.path());
        }
    }
    return totalSize;
}

std::vector<BatchWithDestination> generateBatches(
    int numEntries,
    int numSpecificFiles,
    int batchSize,
    int payloadSize)
{
    std::vector<BatchWithDestination> batches;

    // Generate specific filenames
    std::vector<std::string> specificFilenames;
    for (int i = 0; i < numSpecificFiles; i++)
    {
        specificFilenames.push_back("specific_log_file" + std::to_string(i + 1) + ".log");
    }

    int totalChoices = numSpecificFiles + 1; // +1 for default (std::nullopt)
    int generated = 0;
    int destinationIndex = 0;

    // Random number generation setup
    std::random_device rd;
    std::mt19937 rng(rd());

    // Define pools similar to compressionRatio.cpp
    std::vector<std::string> userIds;
    for (int i = 1; i <= 1000; ++i)
    {
        userIds.push_back("user_" + std::to_string(i));
    }

    std::vector<std::string> attributes = {
        "profile", "settings", "history", "preferences", "contacts",
        "messages", "photos", "documents", "videos", "audio"};

    std::vector<std::string> controllerIds;
    for (int i = 1; i <= 10; ++i)
    {
        controllerIds.push_back("controller_" + std::to_string(i));
    }

    std::vector<std::string> processorIds;
    for (int i = 1; i <= 20; ++i)
    {
        processorIds.push_back("processor_" + std::to_string(i));
    }

    std::vector<std::string> wordList = {
        "the", "data", //"to", "and", "user","is", "in", "for", "of", "access",
        //"system", "time", "log", "with", "on", "from", "request", "error", "file", "server",
        //"update", "status", "by", "at", "process", "information", "new", "this", "connection", "failed",
        //"success", "operation", "id", "network", "event", "application", "check", "value", "into", "service",
        //"query", "response", "get", "set", "action", "report", "now", "client", "device", "start"
    };

    // Zipfian distribution for payload words
    std::vector<double> weights;
    for (size_t k = 0; k < wordList.size(); ++k)
    {
        weights.push_back(1.0 / (k + 1.0));
    }
    std::discrete_distribution<size_t> wordDist(weights.begin(), weights.end());

    // Generate power-of-2 sizes for variable payload
    std::vector<size_t> powerOf2Sizes;
    int minPowerOf2 = 5; // 2^5 = 32
    int maxPowerOf2 = static_cast<int>(std::log2(payloadSize));
    for (int power = minPowerOf2; power <= maxPowerOf2; power++)
    {
        powerOf2Sizes.push_back(1 << power); // 2^power
    }

    // Distributions for random selections
    std::uniform_int_distribution<int> actionDist(0, 3); // CREATE, READ, UPDATE, DELETE
    std::uniform_int_distribution<size_t> userDist(0, userIds.size() - 1);
    std::uniform_int_distribution<size_t> attrDist(0, attributes.size() - 1);
    std::uniform_int_distribution<size_t> controllerDist(0, controllerIds.size() - 1);
    std::uniform_int_distribution<size_t> processorDist(0, processorIds.size() - 1);
    std::uniform_int_distribution<size_t> powerOf2SizeDist(0, powerOf2Sizes.size() - 1);

    while (generated < numEntries)
    {
        int currentBatchSize = std::min(batchSize, numEntries - generated);

        // Assign destination in round-robin manner
        std::optional<std::string> targetFilename = std::nullopt;
        if (destinationIndex % totalChoices > 0)
        {
            targetFilename = specificFilenames[(destinationIndex % totalChoices) - 1];
        }

        // Generate the batch
        std::vector<LogEntry> batch;
        batch.reserve(currentBatchSize);
        for (int i = 0; i < currentBatchSize; i++)
        {
            // Generate realistic log entry
            auto action = static_cast<LogEntry::ActionType>(actionDist(rng));
            std::string user_id = userIds[userDist(rng)];
            std::string attribute = attributes[attrDist(rng)];
            std::string dataLocation = "user/" + user_id + "/" + attribute;
            std::string dataSubjectId = user_id;
            std::string dataControllerId = controllerIds[controllerDist(rng)];
            std::string dataProcessorId = processorIds[processorDist(rng)];

            // Determine targetSize
            size_t targetSize = static_cast<size_t>(payloadSize);

            // Build payload
            std::string payloadStr;
            while (payloadStr.size() < targetSize)
            {
                if (!payloadStr.empty())
                    payloadStr += " ";
                size_t wordIndex = wordDist(rng);
                payloadStr += wordList[wordIndex];
            }
            if (payloadStr.size() > targetSize)
            {
                payloadStr = payloadStr.substr(0, targetSize);
            }
            std::vector<uint8_t> payload(payloadStr.begin(), payloadStr.end());

            LogEntry entry(action,
                           dataLocation,
                           dataControllerId,
                           dataProcessorId,
                           dataSubjectId,
                           std::move(payload));
            batch.push_back(std::move(entry));
        }

        batches.push_back({std::move(batch), targetFilename});
        generated += currentBatchSize;
        destinationIndex++; // Move to the next destination
    }

    return batches;
}

LatencyStats calculateLatencyStats(const LatencyCollector &collector)
{
    const auto &latencies = collector.getMeasurements();

    if (latencies.empty())
    {
        return {0.0, 0.0, 0.0, 0};
    }

    // Convert to milliseconds for easier reading
    std::vector<double> latenciesMs;
    latenciesMs.reserve(latencies.size());
    for (const auto &lat : latencies)
    {
        latenciesMs.push_back(static_cast<double>(lat.count()) / 1e6); // ns to ms
    }

    // Sort for percentile calculations
    std::sort(latenciesMs.begin(), latenciesMs.end());

    LatencyStats stats;
    stats.count = latenciesMs.size();
    stats.maxMs = latenciesMs.back();
    stats.avgMs = std::accumulate(latenciesMs.begin(), latenciesMs.end(), 0.0) / latenciesMs.size();

    // Median
    size_t medianIdx = latenciesMs.size() / 2;
    if (latenciesMs.size() % 2 == 0)
    {
        stats.medianMs = (latenciesMs[medianIdx - 1] + latenciesMs[medianIdx]) / 2.0;
    }
    else
    {
        stats.medianMs = latenciesMs[medianIdx];
    }

    return stats;
}

void printLatencyStats(const LatencyStats &stats)
{
    std::cout << "============== Latency Statistics ==============" << std::endl;
    std::cout << "Total append operations: " << stats.count << std::endl;
    std::cout << "Max latency: " << stats.maxMs << " ms" << std::endl;
    std::cout << "Average latency: " << stats.avgMs << " ms" << std::endl;
    std::cout << "Median latency: " << stats.medianMs << " ms" << std::endl;
    std::cout << "===============================================" << std::endl;
}