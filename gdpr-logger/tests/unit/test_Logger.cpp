#include <gtest/gtest.h>
#include "Logger.hpp"
#include "BufferQueue.hpp"
#include <chrono>
#include <thread>

class LoggerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Create a fresh instance for each test
        Logger::s_instance.reset();

        // Create a BufferQueue instance
        queue = std::make_shared<BufferQueue>(1024, 10);
    }

    void TearDown() override
    {
        // Clean up the singleton
        Logger::s_instance.reset();
    }

    std::shared_ptr<BufferQueue> queue;
};

// Test getInstance returns the same instance
TEST_F(LoggerTest, GetInstanceReturnsSingleton)
{
    Logger &instance1 = Logger::getInstance();
    Logger &instance2 = Logger::getInstance();

    EXPECT_EQ(&instance1, &instance2);
}

// Test initialization with valid queue
TEST_F(LoggerTest, InitializeWithValidQueue)
{
    Logger &logger = Logger::getInstance();

    EXPECT_TRUE(logger.initialize(queue));
    EXPECT_TRUE(logger.reset());
}

// Test initialization with null queue
TEST_F(LoggerTest, InitializeWithNullQueue)
{
    Logger &logger = Logger::getInstance();

    EXPECT_FALSE(logger.initialize(nullptr));
}

// Test double initialization
TEST_F(LoggerTest, DoubleInitialization)
{
    Logger &logger = Logger::getInstance();

    EXPECT_TRUE(logger.initialize(queue));
    EXPECT_FALSE(logger.initialize(queue));

    EXPECT_TRUE(logger.reset());
}

// Test creating producer token
TEST_F(LoggerTest, CreateProducerToken)
{
    Logger &logger = Logger::getInstance();

    // Should throw when not initialized
    EXPECT_THROW(logger.createProducerToken(), std::runtime_error);

    EXPECT_TRUE(logger.initialize(queue));

    // Should not throw when initialized
    EXPECT_NO_THROW({
        BufferQueue::ProducerToken token = logger.createProducerToken();
    });

    EXPECT_TRUE(logger.reset());
}

// Test appending log entry after initialization
TEST_F(LoggerTest, AppendAfterInitialization)
{
    Logger &logger = Logger::getInstance();
    EXPECT_TRUE(logger.initialize(queue));

    BufferQueue::ProducerToken token = logger.createProducerToken();
    LogEntry entry(LogEntry::ActionType::READ, "location", "controller", "processor", "subject");

    EXPECT_TRUE(logger.append(std::move(entry), token));
    EXPECT_TRUE(logger.reset());
}

// Test blocking append with queue eventually emptying
TEST_F(LoggerTest, BlockingAppendWithConsumption)
{
    Logger &logger = Logger::getInstance();
    auto smallQueue = std::make_shared<BufferQueue>(2, 1);
    EXPECT_TRUE(logger.initialize(smallQueue, std::chrono::milliseconds(1000)));

    BufferQueue::ProducerToken token = logger.createProducerToken();

    // Since queue grows dynamically, we'll test timeout instead
    LogEntry entry1(LogEntry::ActionType::READ, "location1", "controller1", "processor1", "subject1");
    EXPECT_TRUE(logger.append(std::move(entry1), token));

    LogEntry entry2(LogEntry::ActionType::READ, "location2", "controller2", "processor2", "subject2");
    // With dynamic queue, this will succeed immediately
    auto start = std::chrono::steady_clock::now();
    EXPECT_TRUE(logger.append(std::move(entry2), token));
    auto end = std::chrono::steady_clock::now();

    // Verify it doesn't block since queue can grow
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    EXPECT_LT(duration, 100); // Should be very fast

    // Verify both items are in the queue
    EXPECT_EQ(smallQueue->size(), 2);

    EXPECT_TRUE(logger.reset());
}

// Test append timeout behavior (new test)
TEST_F(LoggerTest, AppendTimeoutBehavior)
{
    Logger &logger = Logger::getInstance();
    auto queue = std::make_shared<BufferQueue>(1024, 1);

    // Initialize with a very short timeout
    EXPECT_TRUE(logger.initialize(queue, std::chrono::milliseconds(50)));

    BufferQueue::ProducerToken token = logger.createProducerToken();
    LogEntry entry(LogEntry::ActionType::READ, "location", "controller", "processor", "subject");

    auto start = std::chrono::steady_clock::now();
    EXPECT_TRUE(logger.append(std::move(entry), token)); // Should succeed immediately since queue grows
    auto end = std::chrono::steady_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    EXPECT_LT(duration, 10); // Very fast operation

    EXPECT_TRUE(logger.reset());
}

// Test batch append functionality
TEST_F(LoggerTest, AppendBatch)
{
    Logger &logger = Logger::getInstance();
    EXPECT_TRUE(logger.initialize(queue));

    BufferQueue::ProducerToken token = logger.createProducerToken();

    std::vector<LogEntry> entries;
    for (int i = 0; i < 5; i++)
    {
        entries.emplace_back(
            LogEntry::ActionType::READ,
            "location_" + std::to_string(i),
            "controller",
            "processor",
            "subject_" + std::to_string(i));
    }

    EXPECT_TRUE(logger.appendBatch(std::move(entries), token));
    EXPECT_EQ(queue->size(), 5);

    // Test empty batch
    std::vector<LogEntry> emptyEntries;
    EXPECT_TRUE(logger.appendBatch(std::move(emptyEntries), token));

    EXPECT_TRUE(logger.reset());
}

// Test shutdown without initialization
TEST_F(LoggerTest, ShutdownWithoutInitialization)
{
    Logger &logger = Logger::getInstance();

    EXPECT_FALSE(logger.reset());
}

// Test shutdown with wait for completion
TEST_F(LoggerTest, ShutdownWithWait)
{
    Logger &logger = Logger::getInstance();
    EXPECT_TRUE(logger.initialize(queue));

    BufferQueue::ProducerToken token = logger.createProducerToken();
    LogEntry entry(LogEntry::ActionType::READ, "location", "controller", "processor", "subject");
    EXPECT_TRUE(logger.append(std::move(entry), token));

    // Launch an asynchronous consumer that waits briefly before draining the queue.
    std::thread consumer([this]()
                         {
        std::this_thread::sleep_for(std::chrono::milliseconds(500)); // simulate delay
        BufferQueue::ConsumerToken consumerToken = queue->createConsumerToken();
        QueueItem dummyItem;
        while (queue->tryDequeue(dummyItem, consumerToken))
        {
        } });

    EXPECT_TRUE(logger.reset());
    consumer.join();
    EXPECT_TRUE(queue->size() == 0);
}

// Test thread safety of singleton
TEST_F(LoggerTest, ThreadSafetySingleton)
{
    std::vector<std::thread> threads;
    std::vector<Logger *> instances(10);

    for (int i = 0; i < 10; i++)
    {
        threads.emplace_back([i, &instances]()
                             { instances[i] = &Logger::getInstance(); });
    }

    for (auto &t : threads)
    {
        t.join();
    }

    // All threads should get the same instance
    for (int i = 1; i < 10; i++)
    {
        EXPECT_EQ(instances[0], instances[i]);
    }
}

// Test thread safety of API operations
TEST_F(LoggerTest, ThreadSafetyOperations)
{
    Logger &logger = Logger::getInstance();
    EXPECT_TRUE(logger.initialize(queue));

    std::vector<std::thread> threads;
    for (int i = 0; i < 10; i++)
    {
        threads.emplace_back([&logger, i]()
                             {
                                 // Create producer token for this thread
                                 BufferQueue::ProducerToken token = logger.createProducerToken();

                                 // Each thread appends 10 entries
                                 for (int j = 0; j < 10; j++) {
                                     LogEntry entry(
                                         LogEntry::ActionType::READ,
                                         "location_" + std::to_string(i),
                                         "controller_" + std::to_string(i),
                                         "processor_" + std::to_string(i),
                                         "subject_" + std::to_string(j)
                                        );
                                     EXPECT_TRUE(logger.append(std::move(entry), token));
                                 } });
    }

    for (auto &t : threads)
    {
        t.join();
    }

    EXPECT_EQ(queue->size(), 100);
}

// Main function that runs all the tests
int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}