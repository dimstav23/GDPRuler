#ifndef WRITER_HPP
#define WRITER_HPP

#include <thread>
#include <atomic>
#include <memory>
#include <vector>
#include "QueueItem.hpp"
#include "BufferQueue.hpp"
#include "SegmentedStorage.hpp"
#include "TrustedCounter.hpp"

class Writer
{
public:
    explicit Writer(BufferQueue &queue,
                    std::shared_ptr<SegmentedStorage> storage,
                    std::shared_ptr<TrustedCounter> trustedCounter,
                    size_t batchSize = 100,
                    bool useEncryption = true,
                    int compressionLevel = 3);

    ~Writer();

    void start();
    void startGDPR();
    void stop();
    bool isRunning() const;

private:
    void processLogEntries();
    void processLogEntriesGDPR();

    BufferQueue &m_queue;
    std::shared_ptr<SegmentedStorage> m_storage;
    std::shared_ptr<TrustedCounter> m_trustedCounter;
    std::unique_ptr<std::thread> m_writerThread;
    std::atomic<bool> m_running{false};
    const size_t m_batchSize;
    const bool m_useEncryption;
    const int m_compressionLevel;

    BufferQueue::ConsumerToken m_consumerToken;
};
#endif