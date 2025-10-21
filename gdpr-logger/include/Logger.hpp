#ifndef LOGGER_HPP
#define LOGGER_HPP

#include "LogEntry.hpp"
#include "BufferQueue.hpp"
#include "QueueItem.hpp"
#include <string>
#include <chrono>
#include <memory>
#include <vector>
#include <shared_mutex>
#include <functional>
#include <optional>

class Logger
{
    friend class LoggerTest;

public:
    static Logger &getInstance();

    bool initialize(std::shared_ptr<BufferQueue> queue,
                    std::chrono::milliseconds appendTimeout = std::chrono::milliseconds::max());

    BufferQueue::ProducerToken createProducerToken();
    bool append(LogEntry entry,
                BufferQueue::ProducerToken &token,
                const std::optional<std::string> &filename = std::nullopt);
    bool appendBatch(std::vector<LogEntry> entries,
                     BufferQueue::ProducerToken &token,
                     const std::optional<std::string> &filename = std::nullopt);

    bool reset();

    ~Logger();

private:
    Logger();
    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;
    // Singleton instance
    static std::unique_ptr<Logger> s_instance;
    static std::mutex s_instanceMutex;

    std::shared_ptr<BufferQueue> m_logQueue;
    std::chrono::milliseconds m_appendTimeout;

    // State tracking
    bool m_initialized;

    // Helper to report errors
    void reportError(const std::string &message);
};

#endif