#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include <chrono>

struct LoggingConfig
{
    // api
    std::chrono::milliseconds appendTimeout = std::chrono::milliseconds(30000);
    // queue
    size_t queueCapacity = 8192;
    size_t maxExplicitProducers = 16; // maximum number of producers creating a producer token
    // writers
    size_t batchSize = 100;
    size_t numWriterThreads = 2;
    bool useEncryption = true;
    int compressionLevel = 9; // 0 = no compression, 1-9 = compression levels
    // segmented storage
    std::string basePath = "./logs";
    std::string baseFilename = "default";
    size_t maxSegmentSize = 100 * 1024 * 1024; // 100 MB
    size_t maxAttempts = 10;
    std::chrono::milliseconds baseRetryDelay = std::chrono::milliseconds(1);
    size_t maxOpenFiles = 512;
};

#endif