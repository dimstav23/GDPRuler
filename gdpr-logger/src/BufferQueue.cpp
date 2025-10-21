#include "BufferQueue.hpp"
#include <algorithm>
#include <thread>
#include <iostream>
#include <chrono>
#include <cmath>

BufferQueue::BufferQueue(size_t capacity, size_t maxExplicitProducers)
{
    m_queue = moodycamel::ConcurrentQueue<QueueItem>(capacity, maxExplicitProducers, 0);
}

bool BufferQueue::enqueue(QueueItem item, ProducerToken &token)
{
    return m_queue.try_enqueue(token, std::move(item));
}

bool BufferQueue::enqueueBlocking(QueueItem item, ProducerToken &token, std::chrono::milliseconds timeout)
{
    auto start = std::chrono::steady_clock::now();
    int backoffMs = 1;
    const int maxBackoffMs = 100;

    while (true)
    {
        QueueItem itemCopy = item;
        if (enqueue(std::move(itemCopy), token))
        {
            return true;
        }

        auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed >= timeout)
        {
            return false;
        }

        int sleepTime = backoffMs;

        // Make sure we don't sleep longer than our remaining timeout
        if (timeout != std::chrono::milliseconds::max())
        {
            auto remainingTime = timeout - elapsed;
            if (remainingTime <= std::chrono::milliseconds(sleepTime))
            {
                sleepTime = std::max(1, static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(remainingTime).count()));
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime));
        backoffMs = std::min(backoffMs * 2, maxBackoffMs);
    }
}

bool BufferQueue::enqueueBatch(std::vector<QueueItem> items, ProducerToken &token)
{
    return m_queue.try_enqueue_bulk(token, std::make_move_iterator(items.begin()), items.size());
}

bool BufferQueue::enqueueBatchBlocking(std::vector<QueueItem> items, ProducerToken &token,
                                       std::chrono::milliseconds timeout)
{
    auto start = std::chrono::steady_clock::now();
    int backoffMs = 1;
    const int maxBackoffMs = 100;

    while (true)
    {
        std::vector<QueueItem> itemsCopy = items;
        if (enqueueBatch(std::move(itemsCopy), token))
        {
            return true;
        }

        auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed >= timeout)
        {
            return false;
        }

        int sleepTime = backoffMs;

        // Make sure we don't sleep longer than our remaining timeout
        if (timeout != std::chrono::milliseconds::max())
        {
            auto remainingTime = timeout - elapsed;
            if (remainingTime <= std::chrono::milliseconds(sleepTime))
            {
                sleepTime = std::max(1, static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(remainingTime).count()));
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime));
        backoffMs = std::min(backoffMs * 2, maxBackoffMs);
    }
}

bool BufferQueue::tryDequeue(QueueItem &item, ConsumerToken &token)
{
    if (m_queue.try_dequeue(token, item))
    {
        return true;
    }
    return false;
}

size_t BufferQueue::tryDequeueBatch(std::vector<QueueItem> &items, size_t maxItems, ConsumerToken &token)
{
    items.clear();
    items.resize(maxItems);

    size_t dequeued = m_queue.try_dequeue_bulk(token, items.begin(), maxItems);
    items.resize(dequeued);

    return dequeued;
}

bool BufferQueue::flush()
{
    do
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    } while (m_queue.size_approx() != 0);

    return true;
}

size_t BufferQueue::size() const
{
    return m_queue.size_approx();
}