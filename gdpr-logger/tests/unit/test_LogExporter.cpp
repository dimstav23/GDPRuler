#include <gtest/gtest.h>
#include "LogExporter.hpp"
#include "SegmentedStorage.hpp"
#include "LogEntry.hpp"
#include "Crypto.hpp"
#include "Compression.hpp"
#include <thread>
#include <vector>
#include <fstream>
#include <filesystem>
#include <random>
#include <algorithm>
#include <chrono>
#include <bitset>

class LogExporterTest : public ::testing::Test
{
protected:
    std::string testPath;
    std::string baseFilename;
    std::shared_ptr<SegmentedStorage> storage;
    std::unique_ptr<LogExporter> exporter;
    bool useEncryption;
    int compressionLevel;

    void SetUp() override
    {
        testPath = "./test_export_";
        baseFilename = "test_export";
        useEncryption = true;
        compressionLevel = 3;

        // Clean up any existing test files
        if (std::filesystem::exists(testPath)) {
            std::filesystem::remove_all(testPath);
        }

        // Create storage and exporter
        storage = std::make_shared<SegmentedStorage>(
            testPath, baseFilename, 1024 * 1024); // 1MB segments for testing
        
        exporter = std::make_unique<LogExporter>(storage, useEncryption, compressionLevel);
    }

    void TearDown() override
    {
        exporter.reset();
        storage.reset();
        
        // Clean up test files
        if (std::filesystem::exists(testPath)) {
            std::filesystem::remove_all(testPath);
        }
    }

    // Helper to create a test log entry
    LogEntry createTestLogEntry(const std::string& key, 
                               uint64_t timestamp,
                               const std::string& gdprKey,
                               uint8_t operation = 1, // get
                               bool valid = true,
                               const std::string& payload = "") {
        (void)key;

        std::bitset<128> userKey; // Assuming num_users = 128
        userKey.set(0); // Set first bit for test
        
        uint8_t operationResult = (operation & 0x07) << 1;
        operationResult |= (valid ? 0x01 : 0x00);
        
        std::vector<uint8_t> payloadBytes;
        if (!payload.empty()) {
            payloadBytes.assign(payload.begin(), payload.end());
        }
        
        return LogEntry(timestamp, gdprKey, userKey, operationResult, std::move(payloadBytes));
    }

    // Helper to write test data to storage
    void writeTestData(const std::string& key, 
                      std::vector<LogEntry> entries,
                      uint32_t batchCounter = 0) {
        
        // Serialize entries
        std::vector<uint8_t> serializedData = LogEntry::serializeBatchGDPR(std::move(entries));
        
        // Prepend counter header
        std::vector<uint8_t> dataWithCounter(sizeof(uint32_t) + serializedData.size());
        std::memcpy(dataWithCounter.data(), &batchCounter, sizeof(uint32_t));
        std::memcpy(dataWithCounter.data() + sizeof(uint32_t), serializedData.data(), serializedData.size());
        
        // Use the data with counter for compression/encryption
        std::vector<uint8_t> processedData = std::move(dataWithCounter);

        // Compress if needed
        if (compressionLevel > 0) {
            processedData = Compression::compress(std::move(processedData), compressionLevel);
        }
        // Encrypt if needed
        if (useEncryption) {
            Crypto crypto;
            std::vector<uint8_t> encryptionKey(crypto.KEY_SIZE, 0x42);
            std::vector<uint8_t> dummyIV(crypto.GCM_IV_SIZE, 0x24);
            processedData = crypto.encrypt(std::move(processedData), encryptionKey, dummyIV);
        } else {
          // Add size header for unencrypted data
          size_t dataSize = processedData.size();
          std::vector<uint8_t> sizedData(sizeof(uint32_t) + dataSize);
          std::memcpy(sizedData.data(), &dataSize, sizeof(uint32_t));
          std::memcpy(sizedData.data() + sizeof(uint32_t), processedData.data(), dataSize);
          processedData = std::move(sizedData);
        }
        
        // Write to storage
        storage->writeToFile(key, std::move(processedData));
        storage->flush();
    }

    // Helper to get current timestamp
    uint64_t getCurrentTimestamp() {
        return std::chrono::system_clock::now().time_since_epoch().count();
    }

    // Helper to get timestamp from hours ago
    uint64_t getTimestampHoursAgo(int hours) {
        auto now = std::chrono::system_clock::now();
        auto pastTime = now - std::chrono::hours(hours);
        return pastTime.time_since_epoch().count();
    }
};

// Test basic export functionality for a single key
TEST_F(LogExporterTest, BasicExportForKey)
{
    std::string testKey = "user123";
    uint64_t timestamp = getCurrentTimestamp();
    
    // Create test entries
    std::vector<LogEntry> entries;
    entries.push_back(createTestLogEntry(testKey, timestamp - 1000, "gdpr_key_1", 1, true, "test_value_1"));
    entries.push_back(createTestLogEntry(testKey, timestamp - 500, "gdpr_key_2", 2, true, "test_value_2"));
    entries.push_back(createTestLogEntry(testKey, timestamp, "gdpr_key_3", 3, false, ""));
    
    // Write test data
    writeTestData(testKey, entries);
    
    // Export logs for key
    auto exportedLogs = exporter->exportLogsForKey(testKey, timestamp + 1000);
    
    ASSERT_EQ(exportedLogs.size(), 3);
    
    // Check that entries contain expected information
    for (const auto& log : exportedLogs) {
        EXPECT_TRUE(log.find("Timestamp:") != std::string::npos);
        EXPECT_TRUE(log.find("User key:") != std::string::npos);
        EXPECT_TRUE(log.find("Operation:") != std::string::npos);
        EXPECT_TRUE(log.find("Result:") != std::string::npos);
    }

    // Check that entries are sorted (first should have earliest timestamp)
    EXPECT_TRUE(exportedLogs[0].find("test_value_1") != std::string::npos);
    EXPECT_TRUE(exportedLogs[1].find("test_value_2") != std::string::npos);
}

// Test export for non-existent key
TEST_F(LogExporterTest, ExportNonExistentKey)
{
    auto exportedLogs = exporter->exportLogsForKey("nonexistent_key");
    EXPECT_TRUE(exportedLogs.empty());
}

// Test timestamp filtering
TEST_F(LogExporterTest, TimestampFiltering)
{
    std::string testKey = "user456";
    int64_t currentTime = getCurrentTimestamp();
    int64_t pastTime = getTimestampHoursAgo(2);
    int64_t futureTime = currentTime + 1000000000; // 1 second in nanoseconds
    
    // Create entries with different timestamps
    std::vector<LogEntry> entries;
    entries.push_back(createTestLogEntry(testKey, pastTime, "old_key", 1, true, "old_entry"));
    entries.push_back(createTestLogEntry(testKey, currentTime, "current_key", 2, true, "current_entry"));
    entries.push_back(createTestLogEntry(testKey, futureTime, "future_key", 3, true, "future_entry"));
    
    writeTestData(testKey, entries);
    
    // Export with timestamp threshold that should exclude future entry
    auto exportedLogs = exporter->exportLogsForKey(testKey, currentTime + 500000000);
    
    ASSERT_EQ(exportedLogs.size(), 2);
    
    // Check that future entry is not included
    bool foundFutureEntry = false;
    for (const auto& log : exportedLogs) {
        if (log.find("future_entry") != std::string::npos) {
            foundFutureEntry = true;
        }
    }
    EXPECT_FALSE(foundFutureEntry);
}

// Test export all logs
TEST_F(LogExporterTest, ExportAllLogs)
{
    uint64_t timestamp = getCurrentTimestamp();
    
    // Create entries for different keys
    std::vector<LogEntry> entries1;
    entries1.push_back(createTestLogEntry("key1", timestamp - 1000, "gdpr_key1", 1, true, "data1"));
    writeTestData("key1", entries1);
    
    std::vector<LogEntry> entries2;
    entries2.push_back(createTestLogEntry("key2", timestamp - 500, "gdpr_key2", 2, true, "data2"));
    entries2.push_back(createTestLogEntry("key2", timestamp, "gdpr_key3", 3, false, "data3"));
    writeTestData("key2", entries2);
    
    // Export all logs
    auto allLogs = exporter->exportAllLogs(timestamp + 1000);
    
    ASSERT_EQ(allLogs.size(), 3);
    
    // Check that logs from both keys are present
    bool foundKey1 = false, foundKey2 = false;
    for (const auto& log : allLogs) {
        if (log.find("data1") != std::string::npos) foundKey1 = true;
        if (log.find("data2") != std::string::npos || log.find("data3") != std::string::npos) foundKey2 = true;
    }
    
    EXPECT_TRUE(foundKey1);
    EXPECT_TRUE(foundKey2);
}

// Test getting list of log files
TEST_F(LogExporterTest, GetLogFilesList)
{
    uint64_t timestamp = getCurrentTimestamp();
    
    // Create some test files
    std::vector<LogEntry> entries;
    entries.push_back(createTestLogEntry("testkey", timestamp, "gdpr_test_key", 1, true, "test"));
    
    writeTestData("file1", entries);
    writeTestData("file2", entries);
    writeTestData("file3", entries);
    
    auto filesList = exporter->getLogFilesList();
    
    // Should have 3 files
    EXPECT_EQ(filesList.size(), 3);
    
    // Files should be sorted
    EXPECT_TRUE(std::is_sorted(filesList.begin(), filesList.end()));
}

// Test operation string conversion
TEST_F(LogExporterTest, OperationStringConversion)
{
    std::string testKey = "operation_test";
    uint64_t timestamp = getCurrentTimestamp();
    
    // Test different operations
    std::vector<LogEntry> entries;
    entries.push_back(createTestLogEntry(testKey, timestamp - 600, "unknown_key", 0, true, ""));  // unknown
    entries.push_back(createTestLogEntry(testKey, timestamp - 500, "get_key", 1, true, ""));      // get
    entries.push_back(createTestLogEntry(testKey, timestamp - 400, "put_key", 2, true, ""));      // put
    entries.push_back(createTestLogEntry(testKey, timestamp - 300, "delete_key", 3, true, ""));   // delete
    entries.push_back(createTestLogEntry(testKey, timestamp - 200, "getM_key", 4, true, ""));     // getM
    entries.push_back(createTestLogEntry(testKey, timestamp - 100, "putM_key", 5, true, ""));     // putM
    entries.push_back(createTestLogEntry(testKey, timestamp, "putC_key", 6, true, ""));           // putC
    entries.push_back(createTestLogEntry(testKey, timestamp, "getLogs_key", 7, true, ""));        // getLogs

    writeTestData(testKey, entries);
    
    auto exportedLogs = exporter->exportLogsForKey(testKey, timestamp + 1000);
    
    ASSERT_EQ(exportedLogs.size(), entries.size());
    
    // Check operation strings
    EXPECT_TRUE(exportedLogs[0].find("unknown") != std::string::npos);
    EXPECT_TRUE(exportedLogs[1].find("get") != std::string::npos);
    EXPECT_TRUE(exportedLogs[2].find("put") != std::string::npos);
    EXPECT_TRUE(exportedLogs[3].find("delete") != std::string::npos);
    EXPECT_TRUE(exportedLogs[4].find("getM") != std::string::npos);
    EXPECT_TRUE(exportedLogs[5].find("putM") != std::string::npos);
    EXPECT_TRUE(exportedLogs[6].find("putC") != std::string::npos);
    EXPECT_TRUE(exportedLogs[7].find("getLogs") != std::string::npos);
}

// Test validity flag formatting
TEST_F(LogExporterTest, ValidityFormatting)
{
    std::string testKey = "validity_test";
    uint64_t timestamp = getCurrentTimestamp();
    
    std::vector<LogEntry> entries;
    entries.push_back(createTestLogEntry(testKey, timestamp - 1000, "valid_key", 1, true, ""));  // valid
    entries.push_back(createTestLogEntry(testKey, timestamp, "invalid_key", 1, false, ""));      // invalid
    
    writeTestData(testKey, entries);
    
    auto exportedLogs = exporter->exportLogsForKey(testKey, timestamp + 1000);
    
    ASSERT_EQ(exportedLogs.size(), 2);
    EXPECT_TRUE(exportedLogs[0].find("Result: valid") != std::string::npos);
    EXPECT_TRUE(exportedLogs[1].find("Result: invalid") != std::string::npos);
}

// Test export with large payloads
TEST_F(LogExporterTest, LargePayloadExport)
{
    std::string testKey = "large_payload_test";
    uint64_t timestamp = getCurrentTimestamp();
    
    // Create entry with large payload
    std::string largePayload(1000, 'A'); // 1000 characters
    std::vector<LogEntry> entries;
    entries.push_back(createTestLogEntry(testKey, timestamp, "large_payload_key", 1, true, largePayload));
    
    writeTestData(testKey, entries);
    
    auto exportedLogs = exporter->exportLogsForKey(testKey, timestamp + 1000);
    
    ASSERT_EQ(exportedLogs.size(), 1);
    EXPECT_TRUE(exportedLogs[0].find("New value:") != std::string::npos);
    EXPECT_TRUE(exportedLogs[0].find(largePayload) != std::string::npos);
}

// Test export to file functionality
TEST_F(LogExporterTest, ExportToFile)
{
    std::string testKey = "file_export_test";
    uint64_t timestamp = getCurrentTimestamp();
    std::string outputPath = testPath + "/exported_logs.txt";
    
    // Create test entries
    std::vector<LogEntry> entries;
    entries.push_back(createTestLogEntry(testKey, timestamp - 1000, "export_key_1", 1, true, "export_test_1"));
    entries.push_back(createTestLogEntry(testKey, timestamp, "export_key_2", 2, false, "export_test_2"));
    
    writeTestData(testKey, entries);
    
    // Export to file
    auto fromTime = std::chrono::system_clock::time_point(std::chrono::nanoseconds(timestamp - 2000));
    auto toTime = std::chrono::system_clock::time_point(std::chrono::nanoseconds(timestamp + 1000));
    
    bool result = exporter->exportToFile(outputPath, fromTime, toTime);
    
    EXPECT_TRUE(result);
    EXPECT_TRUE(std::filesystem::exists(outputPath));
    
    // Read and verify file contents
    std::ifstream file(outputPath);
    std::string line;
    std::vector<std::string> lines;
    
    while (std::getline(file, line)) {
        if (!line.empty()) {
            lines.push_back(line);
        }
    }
    
    EXPECT_EQ(lines.size(), 2);
    
    bool foundTest1 = false, foundTest2 = false;
    for (const auto& line : lines) {
        if (line.find("export_test_1") != std::string::npos) foundTest1 = true;
        if (line.find("export_test_2") != std::string::npos) foundTest2 = true;
    }
    
    EXPECT_TRUE(foundTest1);
    EXPECT_TRUE(foundTest2);
}

// Test error handling with corrupted data
TEST_F(LogExporterTest, CorruptedDataHandling)
{
    std::string testKey = "corrupted_test";
    
    // Write corrupted data directly to storage
    std::vector<uint8_t> corruptedData = {0xFF, 0xFE, 0xFD, 0xFC, 0xFB};
    storage->writeToFile(testKey, std::move(corruptedData));
    storage->flush();
    
    // Should handle gracefully and return empty results
    auto exportedLogs = exporter->exportLogsForKey(testKey);
    EXPECT_TRUE(exportedLogs.empty());
}

// Test concurrent export operations
TEST_F(LogExporterTest, ConcurrentExport)
{
    uint64_t timestamp = getCurrentTimestamp();
    
    // Create test data for multiple keys
    for (int i = 0; i < 5; ++i) {
        std::string key = "concurrent_key_" + std::to_string(i);
        std::string gdprKey = "concurrent_gdpr_key_" + std::to_string(i);
        std::vector<LogEntry> entries;
        entries.push_back(createTestLogEntry(key, timestamp + i, gdprKey, 1, true, "data_" + std::to_string(i)));
        writeTestData(key, entries);
    }
    
    // Export concurrently from multiple threads
    std::vector<std::thread> threads;
    std::vector<std::vector<std::string>> results(5);
    
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([this, &results, i, timestamp]() {
            std::string key = "concurrent_key_" + std::to_string(i);
            results[i] = exporter->exportLogsForKey(key, timestamp + 10000);
        });
    }
    
    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }
    
    // Verify results
    for (int i = 0; i < 5; ++i) {
        ASSERT_EQ(results[i].size(), 1);
        EXPECT_TRUE(results[i][0].find("data_" + std::to_string(i)) != std::string::npos);
    }
}

// Test with unencrypted data
TEST_F(LogExporterTest, UnencryptedData)
{
    // Create exporter without encryption
    auto unencryptedExporter = std::make_unique<LogExporter>(storage, false, compressionLevel);
    
    std::string testKey = "unencrypted_test";
    uint64_t timestamp = getCurrentTimestamp();
    
    // Create test entry
    std::vector<LogEntry> entries;
    entries.push_back(createTestLogEntry(testKey, timestamp, "unencrypted_gdpr_key", 1, true, "unencrypted_data"));

    // Disable encryption
    useEncryption = false;
    writeTestData(testKey, std::move(entries), 0);
    // Re-enable encryption
    useEncryption = true;
    
    // Export should work
    auto exportedLogs = unencryptedExporter->exportLogsForKey(testKey, timestamp + 1000);
    
    ASSERT_EQ(exportedLogs.size(), 1);
    EXPECT_TRUE(exportedLogs[0].find("unencrypted_data") != std::string::npos);
}

// Test with uncompressed data
TEST_F(LogExporterTest, UncompressedData)
{
    // Create exporter without compression
    auto uncompressedExporter = std::make_unique<LogExporter>(storage, useEncryption, 0);
    
    std::string testKey = "uncompressed_test";
    uint64_t timestamp = getCurrentTimestamp();
    
    // Create test entry
    std::vector<LogEntry> entries;
    entries.push_back(createTestLogEntry(testKey, timestamp, "uncompressed_gdpr_key", 1, true, "uncompressed_data"));
    
    // Disable compression
    int oldCompressionLevel = compressionLevel;
    compressionLevel = 0;
    writeTestData(testKey, std::move(entries), 0);
    // Re-enable compression
    compressionLevel = oldCompressionLevel;
    
    // Export should work
    auto exportedLogs = uncompressedExporter->exportLogsForKey(testKey, timestamp + 1000);
    
    ASSERT_EQ(exportedLogs.size(), 1);
    EXPECT_TRUE(exportedLogs[0].find("uncompressed_data") != std::string::npos);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
