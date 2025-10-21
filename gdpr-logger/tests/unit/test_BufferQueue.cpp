#include <gtest/gtest.h>
#include "BufferQueue.hpp"
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <future>
#include <random>

// Basic functionality tests
class BufferQueueBasicTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Create a new queue for each test
        queue = std::make_unique<BufferQueue>(QUEUE_SIZE, 1);
    }

    void TearDown() override
    {
        queue.reset();
    }

    // Helper to create a test queue item with log entry
    QueueItem createTestItem(int id)
    {
        QueueItem item;
        item.entry = LogEntry(
            LogEntry::ActionType::READ,
            "data/location/" + std::to_string(id),
            "controller" + std::to_string(id),
            "processor" + std::to_string(id),
            "subject" + std::to_string(id % 10));
        return item;
    }

    // Helper to create a test queue item with log entry and target filename
    QueueItem createTestItemWithTarget(int id, const std::string &filename)
    {
        QueueItem item = createTestItem(id);
        item.targetFilename = filename;
        return item;
    }

    const size_t QUEUE_SIZE = 1; // leads to capacity being block_size
    const size_t QUEUE_BLOCK_SIZE = 64;
    const size_t QUEUE_CAPACITY = QUEUE_BLOCK_SIZE;
    std::unique_ptr<BufferQueue> queue;
};

// Test enqueue and dequeue operations
TEST_F(BufferQueueBasicTest, EnqueueDequeue)
{
    QueueItem item = createTestItem(1);
    QueueItem retrievedItem;

    // Queue should be empty initially
    EXPECT_EQ(queue->size(), 0);

    BufferQueue::ProducerToken producerToken = queue->createProducerToken();
    BufferQueue::ConsumerToken consumerToken = queue->createConsumerToken();

    // Enqueue one item
    EXPECT_TRUE(queue->enqueueBlocking(item, producerToken, std::chrono::milliseconds(100)));
    EXPECT_EQ(queue->size(), 1);

    // Dequeue the item
    EXPECT_TRUE(queue->tryDequeue(retrievedItem, consumerToken));
    EXPECT_EQ(queue->size(), 0);

    // Verify the item matches
    EXPECT_EQ(retrievedItem.entry.getDataControllerId(), item.entry.getDataControllerId());
    EXPECT_EQ(retrievedItem.entry.getDataProcessorId(), item.entry.getDataProcessorId());
    EXPECT_EQ(retrievedItem.entry.getDataLocation(), item.entry.getDataLocation());
    EXPECT_EQ(retrievedItem.entry.getDataSubjectId(), item.entry.getDataSubjectId());
    EXPECT_EQ(retrievedItem.entry.getActionType(), item.entry.getActionType());
    EXPECT_EQ(retrievedItem.targetFilename, item.targetFilename);
}

TEST_F(BufferQueueBasicTest, EnqueueUntilFull)
{
    // Test now verifies that we can enqueue up to capacity and then operations fail
    BufferQueue::ProducerToken producerToken = queue->createProducerToken();

    // Fill the queue up to capacity
    for (size_t i = 0; i < QUEUE_CAPACITY; i++)
    {
        EXPECT_TRUE(queue->enqueueBlocking(createTestItem(i), producerToken, std::chrono::milliseconds(100)));
    }

    // Queue should be full now
    EXPECT_EQ(queue->size(), QUEUE_CAPACITY);

    // Testing that enqueue fails
    EXPECT_FALSE(queue->enqueueBlocking(createTestItem(123), producerToken));

    // With longer timeout, enqueue should block and eventually fail too since no consumer
    auto start = std::chrono::steady_clock::now();
    auto timeout = std::chrono::milliseconds(50);
    EXPECT_FALSE(queue->enqueueBlocking(createTestItem(123), producerToken, timeout));
    auto elapsed = std::chrono::steady_clock::now() - start;
    EXPECT_GE(elapsed, timeout); // Verify that it blocked for at least the timeout period
}

// Test enqueue with consumer thread
TEST_F(BufferQueueBasicTest, EnqueueWithConsumer)
{
    BufferQueue::ProducerToken producerToken = queue->createProducerToken();

    // Fill the queue to capacity
    for (size_t i = 0; i < QUEUE_CAPACITY; i++)
    {
        EXPECT_TRUE(queue->enqueueBlocking(createTestItem(i), producerToken, std::chrono::milliseconds(100)));
    }

    EXPECT_EQ(queue->size(), QUEUE_CAPACITY);

    // Testing that enqueue fails
    EXPECT_FALSE(queue->enqueueBlocking(createTestItem(123), producerToken));

    BufferQueue::ConsumerToken consumerToken = queue->createConsumerToken();
    QueueItem retrievedItem;
    EXPECT_TRUE(queue->tryDequeue(retrievedItem, consumerToken));
    EXPECT_EQ(retrievedItem.entry.getActionType(), LogEntry::ActionType::READ);
    EXPECT_EQ(retrievedItem.entry.getDataLocation(), "data/location/0");

    // block of size 64 only becomes free again after entire block has been dequeued.
    EXPECT_FALSE(queue->enqueueBlocking(createTestItem(99999), producerToken, std::chrono::seconds(1)));

    EXPECT_EQ(queue->size(), QUEUE_CAPACITY - 1);

    // Batch dequeue
    std::vector<QueueItem> items;
    size_t count = queue->tryDequeueBatch(items, QUEUE_CAPACITY - 1, consumerToken);
    // Verify we got all items
    EXPECT_EQ(count, QUEUE_CAPACITY - 1);
    EXPECT_EQ(items.size(), QUEUE_CAPACITY - 1);
    EXPECT_EQ(queue->size(), 0);

    // now enqueue should work again after entire block has been freed
    EXPECT_TRUE(queue->enqueueBlocking(createTestItem(99999), producerToken, std::chrono::seconds(1)));
}

// Test dequeue from empty queue
TEST_F(BufferQueueBasicTest, DequeueFromEmpty)
{
    BufferQueue::ConsumerToken consumerToken = queue->createConsumerToken();
    QueueItem item;
    EXPECT_FALSE(queue->tryDequeue(item, consumerToken));
}

// Test batch dequeue
TEST_F(BufferQueueBasicTest, BatchDequeue)
{
    const size_t numEntries = 5;

    BufferQueue::ProducerToken producerToken = queue->createProducerToken();
    BufferQueue::ConsumerToken consumerToken = queue->createConsumerToken();

    // Enqueue several items
    for (size_t i = 0; i < numEntries; i++)
    {
        EXPECT_TRUE(queue->enqueueBlocking(createTestItem(i), producerToken, std::chrono::milliseconds(100)));
    }

    // Batch dequeue
    std::vector<QueueItem> items;
    size_t count = queue->tryDequeueBatch(items, numEntries, consumerToken);

    // Verify we got all items
    EXPECT_EQ(count, numEntries);
    EXPECT_EQ(items.size(), numEntries);
    EXPECT_EQ(queue->size(), 0);

    // Verify entries match what we enqueued
    for (size_t i = 0; i < numEntries; i++)
    {
        EXPECT_EQ(items[i].entry.getDataLocation(), "data/location/" + std::to_string(i));
    }
}

// Test batch dequeue with more items requested than available
TEST_F(BufferQueueBasicTest, BatchDequeuePartial)
{
    const size_t numEntries = 3;
    const size_t requestSize = 5;

    BufferQueue::ProducerToken producerToken = queue->createProducerToken();
    BufferQueue::ConsumerToken consumerToken = queue->createConsumerToken();

    // Enqueue a few items
    for (size_t i = 0; i < numEntries; i++)
    {
        EXPECT_TRUE(queue->enqueueBlocking(createTestItem(i), producerToken, std::chrono::milliseconds(100)));
    }

    // Try to dequeue more than available
    std::vector<QueueItem> items;
    size_t count = queue->tryDequeueBatch(items, requestSize, consumerToken);

    // Verify we got what was available
    EXPECT_EQ(count, numEntries);
    EXPECT_EQ(items.size(), numEntries);
    EXPECT_EQ(queue->size(), 0);
}

// Test batch enqueue functionality
TEST_F(BufferQueueBasicTest, BatchEnqueue)
{
    const size_t numEntries = 5;
    std::vector<QueueItem> itemsToEnqueue;

    BufferQueue::ProducerToken producerToken = queue->createProducerToken();
    BufferQueue::ConsumerToken consumerToken = queue->createConsumerToken();

    for (size_t i = 0; i < numEntries; i++)
    {
        itemsToEnqueue.push_back(createTestItem(i));
    }

    EXPECT_TRUE(queue->enqueueBatchBlocking(itemsToEnqueue, producerToken));
    EXPECT_EQ(queue->size(), numEntries);

    std::vector<QueueItem> retrievedItems;
    size_t dequeued = queue->tryDequeueBatch(retrievedItems, numEntries, consumerToken);

    EXPECT_EQ(dequeued, numEntries);
    EXPECT_EQ(retrievedItems.size(), numEntries);

    for (size_t i = 0; i < numEntries; i++)
    {
        EXPECT_EQ(retrievedItems[i].entry.getDataLocation(), itemsToEnqueue[i].entry.getDataLocation());
        EXPECT_EQ(retrievedItems[i].entry.getDataControllerId(), itemsToEnqueue[i].entry.getDataControllerId());
        EXPECT_EQ(retrievedItems[i].entry.getDataProcessorId(), itemsToEnqueue[i].entry.getDataProcessorId());
        EXPECT_EQ(retrievedItems[i].entry.getDataSubjectId(), itemsToEnqueue[i].entry.getDataSubjectId());
    }

    EXPECT_EQ(queue->size(), 0);
}

TEST_F(BufferQueueBasicTest, BatchEnqueueWhenAlmostFull)
{
    BufferQueue::ProducerToken producerToken = queue->createProducerToken();
    BufferQueue::ConsumerToken consumerToken = queue->createConsumerToken();

    // Fill most of the queue
    for (size_t i = 0; i < QUEUE_CAPACITY - 3; i++)
    {
        EXPECT_TRUE(queue->enqueueBlocking(createTestItem(i), producerToken, std::chrono::milliseconds(100)));
    }

    // Queue has 3 spaces left
    std::vector<QueueItem> smallBatch;
    for (size_t i = 0; i < 3; i++)
    {
        smallBatch.push_back(createTestItem(100 + i));
    }

    // This should succeed (exactly fits available space)
    EXPECT_TRUE(queue->enqueueBatchBlocking(smallBatch, producerToken));
    EXPECT_EQ(queue->size(), QUEUE_CAPACITY);

    // Create a batch larger than available space
    std::vector<QueueItem> largeBatch;
    for (size_t i = 0; i < 4; i++)
    {
        largeBatch.push_back(createTestItem(200 + i));
    }

    // This should fail with a short timeout
    EXPECT_FALSE(queue->enqueueBatchBlocking(largeBatch, producerToken, std::chrono::milliseconds(1)));

    // Remove ALL items to make space (to free the entire block)
    std::vector<QueueItem> retrievedItems;
    size_t removed = queue->tryDequeueBatch(retrievedItems, QUEUE_CAPACITY, consumerToken);
    EXPECT_EQ(removed, QUEUE_CAPACITY);
    EXPECT_EQ(queue->size(), 0);

    // Now batch enqueue should succeed
    EXPECT_TRUE(queue->enqueueBatchBlocking(largeBatch, producerToken, std::chrono::milliseconds(100)));
    EXPECT_EQ(queue->size(), 4);
}

TEST_F(BufferQueueBasicTest, BatchEnqueueBlocking)
{
    BufferQueue::ProducerToken producerToken = queue->createProducerToken();

    // Fill the queue to capacity
    for (size_t i = 0; i < QUEUE_CAPACITY; i++)
    {
        EXPECT_TRUE(queue->enqueueBlocking(createTestItem(i), producerToken, std::chrono::milliseconds(100)));
    }
    EXPECT_EQ(queue->size(), QUEUE_CAPACITY);

    std::vector<QueueItem> batch;
    for (size_t i = 0; i < 3; i++)
    {
        batch.push_back(createTestItem(100 + i));
    }

    // Create a producer thread that will block until space is available
    std::atomic<bool> producerSucceeded{false};
    std::thread producerThread([this, &batch, &producerSucceeded, &producerToken]() { // Pass producerToken by reference
        // Use the same producer token instead of creating a new one
        if (queue->enqueueBatchBlocking(batch, producerToken, std::chrono::seconds(1)))
        {
            producerSucceeded.store(true);
        }
    });

    // Give producer thread a chance to start and block
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    // Should still be false because queue is full
    EXPECT_FALSE(producerSucceeded.load());

    // Create a consumer thread to empty the entire queue
    std::thread consumerThread([this]()
                               {
        BufferQueue::ConsumerToken consumerToken = queue->createConsumerToken();
        std::vector<QueueItem> items;
        // Dequeue all items to free the entire block
        queue->tryDequeueBatch(items, QUEUE_CAPACITY, consumerToken); });

    // Wait for both threads
    consumerThread.join();
    producerThread.join();

    // Now producer should have succeeded
    EXPECT_TRUE(producerSucceeded.load());
    EXPECT_EQ(queue->size(), 3);
}

// Test flush method
TEST_F(BufferQueueBasicTest, Flush)
{
    BufferQueue::ProducerToken producerToken = queue->createProducerToken();

    const size_t numEntries = 5;
    // Enqueue several items
    for (size_t i = 0; i < numEntries; i++)
    {
        EXPECT_TRUE(queue->enqueueBlocking(createTestItem(i), producerToken, std::chrono::milliseconds(100)));
    }

    // Start a thread to dequeue all items
    std::thread consumer([&]
                         {
        BufferQueue::ConsumerToken consumerToken = queue->createConsumerToken();
        std::vector<QueueItem> items;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        queue->tryDequeueBatch(items, numEntries, consumerToken); });

    // Flush should wait until queue is empty
    EXPECT_TRUE(queue->flush());
    EXPECT_EQ(queue->size(), 0);

    consumer.join();
}

// Test for QueueItem with targetFilename
TEST_F(BufferQueueBasicTest, QueueItemWithTargetFilename)
{
    BufferQueue::ProducerToken producerToken = queue->createProducerToken();
    BufferQueue::ConsumerToken consumerToken = queue->createConsumerToken();
    // Create items with target filenames
    QueueItem item1 = createTestItemWithTarget(1, "file1.log");
    QueueItem item2 = createTestItemWithTarget(2, "file2.log");
    QueueItem item3 = createTestItem(3); // No target filename

    // Enqueue items
    EXPECT_TRUE(queue->enqueueBlocking(item1, producerToken, std::chrono::milliseconds(100)));
    EXPECT_TRUE(queue->enqueueBlocking(item2, producerToken, std::chrono::milliseconds(100)));
    EXPECT_TRUE(queue->enqueueBlocking(item3, producerToken, std::chrono::milliseconds(100)));

    // Dequeue and verify
    QueueItem retrievedItem1, retrievedItem2, retrievedItem3;

    EXPECT_TRUE(queue->tryDequeue(retrievedItem1, consumerToken));
    EXPECT_TRUE(queue->tryDequeue(retrievedItem2, consumerToken));
    EXPECT_TRUE(queue->tryDequeue(retrievedItem3, consumerToken));

    // Check targetFilename is preserved correctly
    EXPECT_TRUE(retrievedItem1.targetFilename.has_value());
    EXPECT_EQ(*retrievedItem1.targetFilename, "file1.log");

    EXPECT_TRUE(retrievedItem2.targetFilename.has_value());
    EXPECT_EQ(*retrievedItem2.targetFilename, "file2.log");

    EXPECT_FALSE(retrievedItem3.targetFilename.has_value());
}

// Thread safety tests
class BufferQueueThreadTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Create a new queue for each test with larger capacity
        queue = std::make_unique<BufferQueue>(QUEUE_CAPACITY - 1, 8);
    }

    void TearDown() override
    {
        queue.reset();
    }

    // Helper to create a test queue item with log entry
    QueueItem createTestItem(int id)
    {
        QueueItem item;
        item.entry = LogEntry(
            LogEntry::ActionType::READ,
            "data/location/" + std::to_string(id),
            "controller" + std::to_string(id),
            "processor" + std::to_string(id),
            "subject" + std::to_string(id % 10));
        return item;
    }

    const size_t QUEUE_BLOCK_SIZE = 64;
    const size_t QUEUE_CAPACITY = 4096;
    std::unique_ptr<BufferQueue> queue;
};

// Test dynamic queue behavior - Modified for non-growing queue
TEST_F(BufferQueueThreadTest, QueueCapacityTest)
{
    const size_t SMALL_CAPACITY = 128;
    auto smallQueue = std::make_unique<BufferQueue>(SMALL_CAPACITY - QUEUE_BLOCK_SIZE, 1);
    BufferQueue::ProducerToken smallQueueProducer = smallQueue->createProducerToken();
    BufferQueue::ConsumerToken smallQueueConsumer = smallQueue->createConsumerToken();

    // Fill the queue up to capacity
    for (size_t i = 0; i < SMALL_CAPACITY; i++)
    {
        EXPECT_TRUE(smallQueue->enqueueBlocking(createTestItem(i), smallQueueProducer, std::chrono::milliseconds(100)));
    }

    EXPECT_EQ(smallQueue->size(), SMALL_CAPACITY);

    // Queue is full, enqueue with short timeout should fail
    EXPECT_FALSE(smallQueue->enqueueBlocking(createTestItem(SMALL_CAPACITY),
                                             smallQueueProducer,
                                             std::chrono::milliseconds(1)));

    // Dequeue all items
    std::vector<QueueItem> items;
    size_t count = smallQueue->tryDequeueBatch(items, SMALL_CAPACITY, smallQueueConsumer);
    EXPECT_EQ(count, SMALL_CAPACITY);
    EXPECT_EQ(smallQueue->size(), 0);

    // Now enqueue should succeed
    EXPECT_TRUE(smallQueue->enqueueBlocking(createTestItem(SMALL_CAPACITY),
                                            smallQueueProducer,
                                            std::chrono::milliseconds(100)));
    EXPECT_EQ(smallQueue->size(), 1);
}

// Test multiple producers, single consumer
TEST_F(BufferQueueThreadTest, MultipleProducersSingleConsumer)
{
    const int NUM_PRODUCERS = 4;
    const int ENTRIES_PER_PRODUCER = 1000;
    const int TOTAL_ENTRIES = NUM_PRODUCERS * ENTRIES_PER_PRODUCER;

    std::atomic<int> totalEnqueued(0);
    std::atomic<int> totalDequeued(0);

    // Start consumer thread
    std::thread consumer([&]
                         {
        BufferQueue::ConsumerToken consumerToken = queue->createConsumerToken();
        QueueItem item;
        while (totalDequeued.load() < TOTAL_ENTRIES) {
            if (queue->tryDequeue(item, consumerToken)) {
                totalDequeued++;
            } else {
                std::this_thread::yield();
            }
        } });

    // Start producer threads
    std::vector<std::thread> producers;
    for (int i = 0; i < NUM_PRODUCERS; i++)
    {
        producers.emplace_back([&, i]
                               {
            BufferQueue::ProducerToken producerToken = queue->createProducerToken();
            for (int j = 0; j < ENTRIES_PER_PRODUCER; j++) {
                int id = i * ENTRIES_PER_PRODUCER + j;
                QueueItem item = createTestItem(id);

                // Try until enqueue succeeds
                while (!queue->enqueueBlocking(item, producerToken, std::chrono::milliseconds(100)))
                {
                    std::this_thread::yield();
                }

                totalEnqueued++;
            } });
    }

    // Wait for producers to finish
    for (auto &t : producers)
    {
        t.join();
    }

    // Wait for consumer
    consumer.join();

    // Verify counts
    EXPECT_EQ(totalEnqueued.load(), TOTAL_ENTRIES);
    EXPECT_EQ(totalDequeued.load(), TOTAL_ENTRIES);
    EXPECT_EQ(queue->size(), 0);
}

// Test single producer, multiple consumers
TEST_F(BufferQueueThreadTest, SingleProducerMultipleConsumers)
{
    const int NUM_CONSUMERS = 4;
    const int TOTAL_ENTRIES = 10000;

    std::atomic<int> totalDequeued(0);

    // Start consumer threads
    std::vector<std::thread> consumers;
    for (int i = 0; i < NUM_CONSUMERS; i++)
    {
        consumers.emplace_back([&]
                               {
            BufferQueue::ConsumerToken consumerToken = queue->createConsumerToken();
            QueueItem item;
            while (totalDequeued.load() < TOTAL_ENTRIES) {
                if (queue->tryDequeue(item, consumerToken))
                {
                    totalDequeued++;
                }
                else
                {
                    std::this_thread::yield();
                }
            } });
    }

    BufferQueue::ProducerToken producerToken = queue->createProducerToken();
    // Producer thread
    for (int i = 0; i < TOTAL_ENTRIES; i++)
    {
        QueueItem item = createTestItem(i);

        // Try until enqueue succeeds
        while (!queue->enqueueBlocking(item, producerToken, std::chrono::milliseconds(100)))
        {
            std::this_thread::yield();
        }
    }

    // Wait for consumers
    for (auto &t : consumers)
    {
        t.join();
    }

    // Verify counts
    EXPECT_EQ(totalDequeued.load(), TOTAL_ENTRIES);
    EXPECT_EQ(queue->size(), 0);
}

// Test multiple producers with batch enqueue
TEST_F(BufferQueueThreadTest, MultipleBatchProducers)
{
    const int NUM_PRODUCERS = 4;
    const int BATCHES_PER_PRODUCER = 50;
    const int ENTRIES_PER_BATCH = 20;
    const int TOTAL_ENTRIES = NUM_PRODUCERS * BATCHES_PER_PRODUCER * ENTRIES_PER_BATCH;

    std::atomic<int> totalEnqueued(0);
    std::atomic<int> totalDequeued(0);

    std::thread consumer([&]()
                         {
        BufferQueue::ConsumerToken consumerToken = queue->createConsumerToken();
        std::vector<QueueItem> items;
        while (totalDequeued.load() < TOTAL_ENTRIES) {
            size_t count = queue->tryDequeueBatch(items, 50, consumerToken);
            if (count > 0) {
                totalDequeued += count;
            } else {
                std::this_thread::yield();
            }
        } });

    std::vector<std::thread> producers;
    for (int i = 0; i < NUM_PRODUCERS; i++)
    {
        producers.emplace_back([&, i]()
                               {
            BufferQueue::ProducerToken producerToken = queue->createProducerToken();
            std::vector<QueueItem> batchToEnqueue;

            for (int b = 0; b < BATCHES_PER_PRODUCER; b++) {
                batchToEnqueue.clear();
                for (int j = 0; j < ENTRIES_PER_BATCH; j++) {
                    int id = (i * BATCHES_PER_PRODUCER * ENTRIES_PER_BATCH) +
                             (b * ENTRIES_PER_BATCH) + j;
                    batchToEnqueue.push_back(createTestItem(id));
                }

                queue->enqueueBatchBlocking(batchToEnqueue, producerToken, std::chrono::milliseconds(500));

                totalEnqueued += ENTRIES_PER_BATCH;
            } });
    }

    for (auto &t : producers)
    {
        t.join();
    }

    consumer.join();

    EXPECT_EQ(totalEnqueued.load(), TOTAL_ENTRIES);
    EXPECT_EQ(totalDequeued.load(), TOTAL_ENTRIES);
    EXPECT_EQ(queue->size(), 0);
}

// Test mixed batch and single item operations
TEST_F(BufferQueueThreadTest, MixedBatchOperations)
{
    const int NUM_THREADS = 6;
    const int OPS_PER_THREAD = 200;
    const int MAX_BATCH_SIZE = 10;

    std::atomic<int> totalEnqueued(0);
    std::atomic<int> totalDequeued(0);

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; i++)
    {
        threads.emplace_back([&, i]()
                             {
            BufferQueue::ProducerToken producerToken = queue->createProducerToken();
            BufferQueue::ConsumerToken consumerToken = queue->createConsumerToken();
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> opType(0, 3);  // 0-1: single enqueue, 2: batch enqueue, 3: dequeue
            std::uniform_int_distribution<> batchSize(2, MAX_BATCH_SIZE);

            for (int j = 0; j < OPS_PER_THREAD; j++) {
                int id = i * OPS_PER_THREAD + j;
                int op = opType(gen);

                if (op <= 1 || totalDequeued.load() >= totalEnqueued.load()) {
                    // Single enqueue
                    QueueItem item = createTestItem(id);
                    if (queue->enqueueBlocking(item, producerToken, std::chrono::milliseconds(50))) {
                        totalEnqueued++;
                    }
                } else if (op == 2) {
                    // Batch enqueue
                    int size = batchSize(gen);
                    std::vector<QueueItem> batch;
                    for (int k = 0; k < size; k++) {
                        batch.push_back(createTestItem(id * 1000 + k));
                    }

                    if (queue->enqueueBatchBlocking(batch, producerToken, std::chrono::milliseconds(50))) {
                        totalEnqueued += size;
                    }
                } else {
                    if (gen() % 2 == 0) {
                        // Single dequeue
                        QueueItem item;
                        if (queue->tryDequeue(item, consumerToken)) {
                            totalDequeued++;
                        }
                    } else {
                        // Batch dequeue
                        std::vector<QueueItem> items;
                        size_t count = queue->tryDequeueBatch(items, batchSize(gen), consumerToken);
                        if (count > 0) {
                            totalDequeued += count;
                        }
                    }
                }
            } });
    }

    for (auto &t : threads)
    {
        t.join();
    }

    // Verify that enqueued >= dequeued and size matches the difference
    EXPECT_GE(totalEnqueued.load(), totalDequeued.load());
    EXPECT_EQ(queue->size(), totalEnqueued.load() - totalDequeued.load());

    BufferQueue::ConsumerToken consumerToken = queue->createConsumerToken();
    // Dequeue remaining entries
    std::vector<QueueItem> items;
    while (queue->tryDequeueBatch(items, MAX_BATCH_SIZE, consumerToken) > 0)
    {
        totalDequeued += items.size();
    }

    EXPECT_EQ(totalEnqueued.load(), totalDequeued.load());
    EXPECT_EQ(queue->size(), 0);
}

// Test batch dequeue with multiple threads
TEST_F(BufferQueueThreadTest, BatchDequeueMultipleThreads)
{
    const int NUM_PRODUCERS = 4;
    const int NUM_CONSUMERS = 2;
    const int ENTRIES_PER_PRODUCER = 1000;
    const int TOTAL_ENTRIES = NUM_PRODUCERS * ENTRIES_PER_PRODUCER;
    const int BATCH_SIZE = 100;

    std::atomic<int> totalEnqueued(0);
    std::atomic<int> totalDequeued(0);

    // Start consumer threads
    std::vector<std::thread> consumers;
    for (int i = 0; i < NUM_CONSUMERS; i++)
    {
        consumers.emplace_back([&]
                               {
            BufferQueue::ConsumerToken consumerToken = queue->createConsumerToken();
            std::vector<QueueItem> items;
            while (totalDequeued.load() < TOTAL_ENTRIES) {
                size_t count = queue->tryDequeueBatch(items, BATCH_SIZE, consumerToken);
                if (count > 0) {
                    totalDequeued += count;
                } else {
                    std::this_thread::yield();
                }
            } });
    }

    // Start producer threads
    std::vector<std::thread> producers;
    for (int i = 0; i < NUM_PRODUCERS; i++)
    {
        producers.emplace_back([&, i]
                               {
            BufferQueue::ProducerToken producerToken = queue->createProducerToken();
            for (int j = 0; j < ENTRIES_PER_PRODUCER; j++) {
                int id = i * ENTRIES_PER_PRODUCER + j;
                QueueItem item = createTestItem(id);

                // Try until enqueue succeeds
                while (!queue->enqueueBlocking(item, producerToken, std::chrono::milliseconds(100)))
                {
                    std::this_thread::yield();
                }

                totalEnqueued++;
            } });
    }

    // Wait for producers to finish
    for (auto &t : producers)
    {
        t.join();
    }

    // Wait for consumers
    for (auto &t : consumers)
    {
        t.join();
    }

    // Verify counts
    EXPECT_EQ(totalEnqueued.load(), TOTAL_ENTRIES);
    EXPECT_EQ(totalDequeued.load(), TOTAL_ENTRIES);
    EXPECT_EQ(queue->size(), 0);
}

// Stress test with random operations
TEST_F(BufferQueueThreadTest, RandomizedStressTest)
{
    const int NUM_THREADS = 8;
    const int OPS_PER_THREAD = 5000;

    std::atomic<int> totalEnqueued(0);
    std::atomic<int> totalDequeued(0);

    // Start worker threads
    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; i++)
    {
        threads.emplace_back([&, i]
                             {
            BufferQueue::ProducerToken producerToken = queue->createProducerToken();
            BufferQueue::ConsumerToken consumerToken = queue->createConsumerToken();
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(0, 1);  // 0 for enqueue, 1 for dequeue

            for (int j = 0; j < OPS_PER_THREAD; j++) {
                int id = i * OPS_PER_THREAD + j;

                if (dis(gen) == 0 || totalDequeued.load() >= totalEnqueued.load()) {
                    // Enqueue
                    QueueItem item = createTestItem(id);
                    if (queue->enqueueBlocking(item, producerToken, std::chrono::milliseconds(100)))
                    {
                        totalEnqueued++;
                    }
                } else {
                    // Dequeue
                    QueueItem item;
                    if (queue->tryDequeue(item, consumerToken)) {
                        totalDequeued++;
                    }
                }
            } });
    }

    // Wait for all threads
    for (auto &t : threads)
    {
        t.join();
    }

    // Verify that enqueued >= dequeued and size matches the difference
    EXPECT_GE(totalEnqueued.load(), totalDequeued.load());
    EXPECT_EQ(queue->size(), totalEnqueued.load() - totalDequeued.load());

    // Dequeue remaining entries
    BufferQueue::ConsumerToken consumerToken = queue->createConsumerToken();
    QueueItem item;
    while (queue->tryDequeue(item, consumerToken))
    {
        totalDequeued++;
    }

    // Final verification
    EXPECT_EQ(totalEnqueued.load(), totalDequeued.load());
    EXPECT_EQ(queue->size(), 0);
}

// Test timed operations
class BufferQueueTimingTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Create a new queue for each test
        queue = std::make_unique<BufferQueue>(QUEUE_CAPACITY, 1);
    }

    void TearDown() override
    {
        queue.reset();
    }

    // Helper to create a test queue item with log entry
    QueueItem createTestItem(int id)
    {
        QueueItem item;
        item.entry = LogEntry(
            LogEntry::ActionType::READ,
            "data/location/" + std::to_string(id),
            "controller" + std::to_string(id),
            "processor" + std::to_string(id),
            "subject" + std::to_string(id % 10));
        return item;
    }

    const size_t QUEUE_CAPACITY = 1024;
    std::unique_ptr<BufferQueue> queue;
};

// Test flush timeout
TEST_F(BufferQueueTimingTest, FlushWithTimeout)
{
    BufferQueue::ProducerToken producerToken = queue->createProducerToken();
    BufferQueue::ConsumerToken consumerToken = queue->createConsumerToken();
    // Enqueue some items
    for (int i = 0; i < 10; i++)
    {
        QueueItem item;
        item.entry = LogEntry(
            LogEntry::ActionType::READ,
            "data/location/" + std::to_string(i),
            "controller",
            "processor",
            "subject");
        queue->enqueueBlocking(item, producerToken, std::chrono::milliseconds(100));
    }

    // Start a future to call flush
    auto future = std::async(std::launch::async, [&]
                             { return queue->flush(); });

    // Start a thread to dequeue after a delay
    std::thread consumer([&]
                         {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        std::vector<QueueItem> items;
        queue->tryDequeueBatch(items, 10, consumerToken); });

    // Flush should complete when queue is emptied
    auto status = future.wait_for(std::chrono::milliseconds(750));
    EXPECT_EQ(status, std::future_status::ready);
    EXPECT_TRUE(future.get());

    consumer.join();
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}