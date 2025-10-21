#ifndef BUFFER_QUEUE_HPP
#define BUFFER_QUEUE_HPP

#include "QueueItem.hpp"
#include "concurrentqueue.h"
#include <atomic>
#include <vector>
#include <memory>
#include <condition_variable>
#include <chrono>

class BufferQueue
{
public:
    using ProducerToken = moodycamel::ProducerToken;
    using ConsumerToken = moodycamel::ConsumerToken;

private:
    moodycamel::ConcurrentQueue<QueueItem> m_queue;

public:
    explicit BufferQueue(size_t capacity, size_t maxExplicitProducers);

    ProducerToken createProducerToken() { return ProducerToken(m_queue); }
    ConsumerToken createConsumerToken() { return ConsumerToken(m_queue); }

    bool enqueueBlocking(QueueItem item,
                         ProducerToken &token,
                         std::chrono::milliseconds timeout = std::chrono::milliseconds::max());
    bool enqueueBatchBlocking(std::vector<QueueItem> items,
                              ProducerToken &token,
                              std::chrono::milliseconds timeout = std::chrono::milliseconds::max());
    bool tryDequeue(QueueItem &item, ConsumerToken &token);
    size_t tryDequeueBatch(std::vector<QueueItem> &items, size_t maxItems, ConsumerToken &token);
    bool flush();
    size_t size() const;

    // delete copy/move
    BufferQueue(const BufferQueue &) = delete;
    BufferQueue &operator=(const BufferQueue &) = delete;
    BufferQueue(BufferQueue &&) = delete;
    BufferQueue &operator=(BufferQueue &&) = delete;

private:
    bool enqueue(QueueItem item, ProducerToken &token);
    bool enqueueBatch(std::vector<QueueItem> items, ProducerToken &token);
};

#endif