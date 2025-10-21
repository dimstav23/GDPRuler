#ifndef LOGGING_SYSTEM_HPP
#define LOGGING_SYSTEM_HPP

#include "Config.hpp"
#include "Logger.hpp"
#include "BufferQueue.hpp"
#include "SegmentedStorage.hpp"
#include "Writer.hpp"
#include "LogEntry.hpp"
#include "LogExporter.hpp"
#include "TrustedCounter.hpp"
#include <memory>
#include <vector>
#include <atomic>
#include <mutex>
#include <chrono>
#include <string>
#include <optional>

class LoggingManager
{
public:
    explicit LoggingManager(const LoggingConfig &config);
    ~LoggingManager();

    bool start();
    bool startGDPR();
    bool stop();
    void pauseWorkersDrainAndResume();

    BufferQueue::ProducerToken createProducerToken();
    bool append(LogEntry entry,
                BufferQueue::ProducerToken &token,
                const std::optional<std::string> &filename = std::nullopt);
    bool appendBatch(std::vector<LogEntry> entries,
                     BufferQueue::ProducerToken &token,
                     const std::optional<std::string> &filename = std::nullopt);

    std::shared_ptr<LogExporter> getLogExporter() const {
      return m_logExporter;
    }
    std::shared_ptr<SegmentedStorage> getStorage() const {
      return m_storage;
    }
    std::shared_ptr<TrustedCounter> getTrustedCounter() const {
        return m_trustedCounter;
    }
        
private:
    std::shared_ptr<BufferQueue> m_queue;             // Thread-safe queue for queue items
    std::shared_ptr<SegmentedStorage> m_storage;      // Manages append-only log segments
    std::shared_ptr<LogExporter> m_logExporter;       // For exporting logs
    std::shared_ptr<TrustedCounter> m_trustedCounter; // Manages trusted counters for GDPR logs
    std::vector<std::unique_ptr<Writer>> m_writers;   // Multiple writer threads
    std::atomic<bool> m_running{false};               // System running state
    std::atomic<bool> m_acceptingEntries{false};      // Controls whether new entries are accepted
    std::mutex m_systemMutex;                         // For system-wide operations

    size_t m_numWriterThreads; // Number of writer threads
    size_t m_batchSize;        // Batch size for writers
    bool m_useEncryption;
    int m_compressionLevel;
};

#endif