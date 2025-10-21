#include "LoggingManager.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <future>
#include <optional>
#include <filesystem>
#include <numeric>

int main()
{
    // system parameters
    LoggingConfig config;
    config.basePath = "./logs";
    config.baseFilename = "default";
    config.maxSegmentSize = 1 * 1024 * 1024; // 1 MB
    config.maxAttempts = 5;
    config.baseRetryDelay = std::chrono::milliseconds(1);
    config.queueCapacity = 1000;
    config.maxExplicitProducers = 1;
    config.batchSize = 10;
    config.numWriterThreads = 1;
    config.appendTimeout = std::chrono::seconds(5);
    config.useEncryption = true;
    config.compressionLevel = 4;
    config.maxOpenFiles = 32;

    if (std::filesystem::exists(config.basePath))
    {
        std::filesystem::remove_all(config.basePath);
    }

    LoggingManager loggingManager(config);
    loggingManager.start();

    auto producerToken = loggingManager.createProducerToken();

    LogEntry entry1(LogEntry::ActionType::READ,
                   "users/user01",
                   "controller1",
                   "processor1",
                   "user01");

    loggingManager.append(entry1, producerToken);

    LogEntry entry2(LogEntry::ActionType::UPDATE,
                    "users/user02",
                    "controller2",
                    "processor2",
                    "user02");

    LogEntry entry3(LogEntry::ActionType::DELETE,
                    "users/user03",
                    "controller3",
                    "processor3",
                    "user03");

    std::vector<LogEntry> batch{
        LogEntry(LogEntry::ActionType::UPDATE, "users/user02", "controller2", "processor2", "user02"),
        LogEntry(LogEntry::ActionType::DELETE, "users/user03", "controller3", "processor3", "user03")};

    loggingManager.appendBatch(batch, producerToken);

    loggingManager.stop();

    return 0;
}