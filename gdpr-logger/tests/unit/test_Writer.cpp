#include <gtest/gtest.h>
#include "Writer.hpp"
#include "BufferQueue.hpp"
#include "SegmentedStorage.hpp"
#include "TrustedCounter.hpp"
#include <chrono>
#include <thread>
#include <filesystem>

class WriterTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Create a temporary directory for testing log segments
        testDir = "test_logs";
        std::filesystem::create_directories(testDir);
        queue = std::make_unique<BufferQueue>(8192, 1);
        // Create a SegmentedStorage instance with small segment size for testing
        storage = std::make_shared<SegmentedStorage>(
            testDir,
            "test_logsegment",
            1024 * 1024 // max segment size (e.g., 1 MB for test)
        );
        trustedCounter = std::make_shared<TrustedCounter>();
        batchSize = 100;
        useEncryption = false;
        compressionLevel = 0;
    }

    void TearDown() override
    {
        if (writer)
        {
            writer->stop();
        }
        // Cleanup test directory if desired
        std::filesystem::remove_all(testDir);
    }

    std::unique_ptr<BufferQueue> queue;
    std::shared_ptr<SegmentedStorage> storage;
    std::shared_ptr<TrustedCounter> trustedCounter;
    std::unique_ptr<Writer> writer;
    std::string testDir;
    size_t batchSize;
    bool useEncryption;
    int compressionLevel;
};

// Test that the writer starts and stops correctly
TEST_F(WriterTest, StartAndStop)
{
    writer = std::make_unique<Writer>(*queue, storage, trustedCounter);
    EXPECT_FALSE(writer->isRunning());

    writer->start();
    EXPECT_TRUE(writer->isRunning());

    writer->stop();
    EXPECT_FALSE(writer->isRunning());
}

// Test multiple start calls
TEST_F(WriterTest, MultipleStartCalls)
{
    writer = std::make_unique<Writer>(*queue, storage, trustedCounter);
    writer->start();
    EXPECT_TRUE(writer->isRunning());

    writer->start(); // multiple start calls should not affect the running state
    EXPECT_TRUE(writer->isRunning());

    writer->stop();
    EXPECT_FALSE(writer->isRunning());
}

// Test batch processing with some entries
TEST_F(WriterTest, ProcessBatchEntries)
{
    std::vector<QueueItem> testItems = {
        QueueItem{LogEntry{LogEntry::ActionType::READ, "location1", "controller1", "processor1", "subject1"}},
        QueueItem{LogEntry{LogEntry::ActionType::CREATE, "location2", "controller2", "processor2", "subject2"}},
        QueueItem{LogEntry{LogEntry::ActionType::UPDATE, "location3", "controller3", "processor3", "subject3"}}};

    BufferQueue::ProducerToken producerToken = queue->createProducerToken();

    // Enqueue test entries
    queue->enqueueBatchBlocking(testItems, producerToken, std::chrono::milliseconds(100));

    // Instantiate writer with a batch size equal to number of test items
    writer = std::make_unique<Writer>(*queue, storage, trustedCounter, testItems.size());
    writer->start();

    // Give some time for the writer thread to process the entries.
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Verify that the queue is empty after processing.
    EXPECT_EQ(queue->size(), 0);

    writer->stop();
}

// Test behavior when the queue is empty
TEST_F(WriterTest, EmptyQueue)
{
    EXPECT_EQ(queue->size(), 0);

    writer = std::make_unique<Writer>(*queue, storage, trustedCounter);
    writer->start();

    // Give some time to verify it handles empty queue gracefully
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_EQ(queue->size(), 0);

    writer->stop();
}

// Use same test fixture but with GDPR-specific tests
class WriterGDPRTest : public WriterTest {};

// Test that the writer starts and stops correctly with GDPR mode
TEST_F(WriterGDPRTest, StartAndStopGDPR)
{
    writer = std::make_unique<Writer>(*queue, storage, trustedCounter);
    EXPECT_FALSE(writer->isRunning());

    writer->startGDPR();
    EXPECT_TRUE(writer->isRunning());

    writer->stop();
    EXPECT_FALSE(writer->isRunning());
}

// Test multiple start calls in GDPR mode
TEST_F(WriterGDPRTest, MultipleStartCallsGDPR)
{
    writer = std::make_unique<Writer>(*queue, storage, trustedCounter);
    writer->startGDPR();
    EXPECT_TRUE(writer->isRunning());

    writer->startGDPR(); // multiple start calls should not affect the running state
    EXPECT_TRUE(writer->isRunning());

    writer->stop();
    EXPECT_FALSE(writer->isRunning());
}

// Test batch processing with GDPR LogEntry format
TEST_F(WriterGDPRTest, ProcessGDPRBatchEntries)
{
    // Create GDPR-format LogEntries
    std::bitset<num_users> userMap1;
    userMap1.set(1);
    userMap1.set(64);

    std::bitset<num_users> userMap2;
    userMap2.set(10);
    userMap2.set(127);

    std::bitset<num_users> userMap3;
    userMap3.set(50);

    std::vector<uint8_t> payload1 = {0x01, 0x02, 0x03};
    std::vector<uint8_t> payload2 = {0x04, 0x05, 0x06, 0x07};
    std::vector<uint8_t> payload3 = {0x08, 0x09, 0x0A, 0x0B, 0x0C};

    std::vector<QueueItem> testItems = {
        QueueItem{LogEntry{1234567890, "user_key_1", userMap1, (1 << 1) | 1, payload1}}, // GET operation, valid
        QueueItem{LogEntry{1234567891, "user_key_2", userMap2, (2 << 1) | 1, payload2}}, // PUT operation, valid
        QueueItem{LogEntry{1234567892, "user_key_3", userMap3, (3 << 1) | 0, payload3}}  // DELETE operation, invalid
    };

    BufferQueue::ProducerToken producerToken = queue->createProducerToken();

    // Enqueue GDPR test entries
    queue->enqueueBatchBlocking(testItems, producerToken, std::chrono::milliseconds(100));

    // Instantiate writer with GDPR mode and batch size equal to number of test items
    writer = std::make_unique<Writer>(*queue, storage, trustedCounter, testItems.size());
    writer->startGDPR();

    // Give time for the writer thread to process the entries
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Verify that the queue is empty after processing
    EXPECT_EQ(queue->size(), 0);

    writer->stop();
}

// Test GDPR mode behavior when the queue is empty
TEST_F(WriterGDPRTest, EmptyQueueGDPR)
{
    EXPECT_EQ(queue->size(), 0);

    writer = std::make_unique<Writer>(*queue, storage, trustedCounter);
    writer->startGDPR();

    // Give some time to verify it handles empty queue gracefully
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_EQ(queue->size(), 0);

    writer->stop();
}

// Test GDPR entries with large payloads
TEST_F(WriterGDPRTest, GDPRLargePayloadHandling)
{
    writer = std::make_unique<Writer>(*queue, storage, trustedCounter, batchSize, useEncryption, compressionLevel);
    writer->startGDPR();

    BufferQueue::ProducerToken token = queue->createProducerToken();

    // Create GDPR entries with large payloads
    std::bitset<num_users> userMap1;
    userMap1.set(5);
    userMap1.set(100);

    std::bitset<num_users> userMap2;
    userMap2.set(20);
    userMap2.set(120);

    // Large payload (5KB)
    std::vector<uint8_t> largePayload(5 * 1024, 0xAB);
    
    // Very large payload (50KB) 
    std::vector<uint8_t> veryLargePayload(50 * 1024, 0xCD);

    std::vector<QueueItem> largeItems;
    largeItems.emplace_back(QueueItem{LogEntry{9876543210, "large_payload_user", userMap1, (2 << 1) | 1, largePayload}});
    largeItems.emplace_back(QueueItem{LogEntry{9876543211, "very_large_payload_user", userMap2, (1 << 1) | 1, veryLargePayload}});

    queue->enqueueBatchBlocking(largeItems, token, std::chrono::milliseconds(1000));

    // Wait for processing (longer timeout for large data)
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // Verify processing completed
    EXPECT_EQ(queue->size(), 0);

    // Verify files were created and have substantial size
    size_t totalFileSize = 0;
    for (auto& p : std::filesystem::directory_iterator(testDir)) {
        if (p.is_regular_file() && p.path().string().find("test_logsegment") != std::string::npos) {
            totalFileSize += std::filesystem::file_size(p);
        }
    }
    
    // Should have written at least the payload sizes plus metadata (33 bytes overhead per entry)
    EXPECT_GT(totalFileSize, largePayload.size() + veryLargePayload.size());
    
    writer->stop();
}

// Test GDPR entries with different operation types
TEST_F(WriterGDPRTest, GDPRMixedOperationTypes)
{
    writer = std::make_unique<Writer>(*queue, storage, trustedCounter);
    writer->startGDPR();

    BufferQueue::ProducerToken token = queue->createProducerToken();

    std::vector<QueueItem> mixedItems;
    
    // Different operation types with GDPR format
    std::bitset<num_users> userMap;
    userMap.set(42);

    std::vector<uint8_t> payload = {0x01, 0x02, 0x03, 0x04, 0x05};

    // Invalid operation (0), valid
    mixedItems.emplace_back(QueueItem{LogEntry{1000, "invalid_op_user", userMap, (0 << 1) | 1, {}}});
    
    // GET operation (1), valid  
    mixedItems.emplace_back(QueueItem{LogEntry{1001, "get_user", userMap, (1 << 1) | 1, payload}});
    
    // PUT operation (2), invalid
    mixedItems.emplace_back(QueueItem{LogEntry{1002, "put_user", userMap, (2 << 1) | 0, payload}});
    
    // DELETE operation (3), valid
    mixedItems.emplace_back(QueueItem{LogEntry{1003, "delete_user", userMap, (3 << 1) | 1, {}}});
    
    // GETM operation (4), invalid
    mixedItems.emplace_back(QueueItem{LogEntry{1004, "getm_user", userMap, (4 << 1) | 0, payload}});

    queue->enqueueBatchBlocking(mixedItems, token, std::chrono::milliseconds(500));

    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // Verify all processed
    EXPECT_EQ(queue->size(), 0);

    // Verify segment files created
    bool foundFiles = false;
    for (auto& p : std::filesystem::directory_iterator(testDir)) {
        if (p.is_regular_file() && p.path().string().find("test_logsegment") != std::string::npos) {
            foundFiles = true;
            break;
        }
    }
    EXPECT_TRUE(foundFiles);

    writer->stop();
}

// Test GDPR mode with high-frequency small batches
TEST_F(WriterGDPRTest, GDPRHighFrequencySmallBatches)
{
    const int batchSize = 3;
    writer = std::make_unique<Writer>(*queue, storage, trustedCounter, batchSize);
    writer->startGDPR();

    BufferQueue::ProducerToken token = queue->createProducerToken();

    // Enqueue many small GDPR batches rapidly
    const int numBatches = 15;
    const int itemsPerBatch = 4;
    
    for (int batch = 0; batch < numBatches; ++batch) {
        std::vector<QueueItem> items;
        
        for (int i = 0; i < itemsPerBatch; ++i) {
            std::bitset<num_users> userMap;
            userMap.set(batch + i); // Different user for each item
            
            uint64_t timestamp = 2000000000 + batch * 100 + i;
            std::string gdprKey = "batch_" + std::to_string(batch) + "_user_" + std::to_string(i);
            uint8_t operationValidity = ((i % 4) << 1) | ((batch + i) % 2); // Vary operations and validity
            
            std::vector<uint8_t> payload(10, static_cast<uint8_t>(batch + i));
            
            items.emplace_back(QueueItem{LogEntry{timestamp, gdprKey, userMap, operationValidity, payload}});
        }
        
        queue->enqueueBatchBlocking(items, token, std::chrono::milliseconds(50));
        
        // Small delay between batches
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Wait for all processing to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // Verify all items processed
    EXPECT_EQ(queue->size(), 0);
    
    writer->stop();
}

// Test GDPR mode with maximum user key map values
TEST_F(WriterGDPRTest, GDPRMaxUserKeyMap)
{
    writer = std::make_unique<Writer>(*queue, storage, trustedCounter, batchSize, useEncryption, compressionLevel);
    writer->startGDPR();

    BufferQueue::ProducerToken token = queue->createProducerToken();

    // Create entries with maximum values
    std::bitset<num_users> maxUserMap;
    maxUserMap.flip(); // Set all bits to 1

    std::vector<uint8_t> payload(1000, 0xFF);

    std::vector<QueueItem> maxItems;
    maxItems.emplace_back(QueueItem{LogEntry{INT64_MAX, "max_values_user", maxUserMap, 0xFF, payload}});

    queue->enqueueBatchBlocking(maxItems, token, std::chrono::milliseconds(500));

    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // Verify processed
    EXPECT_EQ(queue->size(), 0);

    // Verify files created
    bool foundFiles = false;
    size_t totalSize = 0;
    for (auto& p : std::filesystem::directory_iterator(testDir)) {
        if (p.is_regular_file() && p.path().string().find("test_logsegment") != std::string::npos) {
            foundFiles = true;
            totalSize += std::filesystem::file_size(p);
        }
    }
    
    EXPECT_TRUE(foundFiles);
    EXPECT_GT(totalSize, payload.size()); // Should be at least payload size + metadata

    writer->stop();
}

// Test concurrent GDPR enqueueing with writer processing
TEST_F(WriterGDPRTest, GDPRConcurrentEnqueuingAndProcessing)
{
    writer = std::make_unique<Writer>(*queue, storage, trustedCounter, 8);
    writer->startGDPR();

    BufferQueue::ProducerToken token = queue->createProducerToken();

    // Start a thread that continuously enqueues GDPR items
    std::atomic<bool> shouldStop{false};
    std::atomic<int> itemsEnqueued{0};
    
    std::thread enqueuingThread([&]() {
        uint32_t counter = 0;
        while (!shouldStop.load()) {
            std::vector<QueueItem> items;
            for (int i = 0; i < 3; ++i) {
                std::bitset<num_users> userMap;
                userMap.set(counter % num_users);
                
                uint64_t timestamp = 3000000000UL + counter;
                std::string gdprKey = "concurrent_user_" + std::to_string(counter);
                uint8_t operationValidity = ((counter % 4) << 1) | (counter % 2);
                std::vector<uint8_t> payload{static_cast<uint8_t>(counter), 
                                           static_cast<uint8_t>(counter + 1)};
                
                items.emplace_back(QueueItem{LogEntry{timestamp, gdprKey,
                                                    userMap, operationValidity, payload}});
                counter++;
            }
            
            if (queue->enqueueBatchBlocking(items, token, std::chrono::milliseconds(10))) {
                itemsEnqueued.fetch_add(items.size());
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(15));
        }
    });

    // Let it run for a while
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    
    // Stop enqueueing
    shouldStop.store(true);
    enqueuingThread.join();

    // Wait for writer to process remaining items
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Verify items were processed
    EXPECT_GT(itemsEnqueued.load(), 0);
    
    // Queue should be empty or nearly empty
    EXPECT_LE(queue->size(), 8); // Allow for some items still in processing

    writer->stop();
}