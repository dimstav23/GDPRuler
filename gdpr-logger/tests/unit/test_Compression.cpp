#include <gtest/gtest.h>
#include "Compression.hpp"
#include "LogEntry.hpp"
#include <vector>
#include <string>
#include <algorithm>

class CompressionTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Create a few sample log entries for testing
        entry1 = LogEntry(LogEntry::ActionType::CREATE, "/data/records/1", "controller123", "processor123", "subject456");
        entry2 = LogEntry(LogEntry::ActionType::READ, "/data/records/2", "controller789", "processor789", "subject456");
        entry3 = LogEntry(LogEntry::ActionType::UPDATE, "/data/records/3", "controller123", "processor123", "subject789");
        entry4 = LogEntry(LogEntry::ActionType::DELETE, "/data/records/4", "controller789", "processor789", "subject123");
    }

    LogEntry entry1, entry2, entry3, entry4;
};

// Helper function to compare two LogEntry objects
bool LogEntriesEqual(const LogEntry &a, const LogEntry &b)
{
    // Compare serialized representations to check equality
    auto serializedA = a.serialize();
    auto serializedB = b.serialize();

    return serializedA == serializedB;
}

// Test compressing and decompressing a batch of log entries
TEST_F(CompressionTest, CompressDecompressBatch)
{
    std::vector<LogEntry> batch = {entry1, entry2, entry3, entry4};
    std::vector<uint8_t> serializedBatch = LogEntry::serializeBatch(std::move(batch));
    std::vector<uint8_t> compressed = Compression::compress(std::move(serializedBatch));

    // Make sure compression produced data
    ASSERT_GT(compressed.size(), 0);

    std::vector<uint8_t> decompressed = Compression::decompress(std::move(compressed));
    std::vector<LogEntry> recoveredBatch = LogEntry::deserializeBatch(std::move(decompressed));

    // Verify we got back the same number of entries
    ASSERT_EQ(batch.size(), recoveredBatch.size());

    // Verify each entry matches
    for (size_t i = 0; i < batch.size(); i++)
    {
        EXPECT_TRUE(LogEntriesEqual(batch[i], recoveredBatch[i]))
            << "Entries at index " << i << " don't match";
    }
}

// Test with an empty batch
TEST_F(CompressionTest, EmptyBatch)
{
    // Create an empty batch
    std::vector<LogEntry> emptyBatch;
    std::vector<uint8_t> serializedBatch = LogEntry::serializeBatch(std::move(emptyBatch));
    std::vector<uint8_t> compressed = Compression::compress(std::move(serializedBatch));

    std::vector<uint8_t> decompressed = Compression::decompress(std::move(compressed));
    std::vector<LogEntry> recoveredBatch = LogEntry::deserializeBatch(std::move(decompressed));

    // Verify we still have an empty vector
    EXPECT_TRUE(recoveredBatch.empty());
}

// Test with invalid compressed data
TEST_F(CompressionTest, InvalidCompressedData)
{
    // Create some invalid compressed data
    std::vector<uint8_t> invalidData = {0x01, 0x02, 0x03, 0x04};

    // Verify that decompression failed
    EXPECT_THROW(
        Compression::decompress(std::move(invalidData)),
        std::runtime_error);
}

// Test batch compression ratio
TEST_F(CompressionTest, BatchCompressionRatio)
{
    // Create a batch of log entries with repetitive data which should compress well
    const int batchSize = 50;
    std::string repetitiveData(1000, 'X');
    LogEntry repetitiveEntry(LogEntry::ActionType::CREATE, repetitiveData, repetitiveData, repetitiveData, repetitiveData);

    std::vector<LogEntry> repetitiveBatch(batchSize, repetitiveEntry);
    std::vector<uint8_t> serializedBatch = LogEntry::serializeBatch(std::move(repetitiveBatch));
    std::vector<uint8_t> compressed = Compression::compress(std::move(serializedBatch));

    // Check that batch compression significantly reduced the size
    double compressionRatio = static_cast<double>(compressed.size()) / static_cast<double>(serializedBatch.size());
    EXPECT_LT(compressionRatio, 0.05); // Expect at least 95% compression for batch

    std::vector<uint8_t> decompressed = Compression::decompress(std::move(compressed));
    std::vector<LogEntry> recoveredBatch = LogEntry::deserializeBatch(std::move(decompressed));
    // Verify the correct number of entries and their content
    ASSERT_EQ(repetitiveBatch.size(), recoveredBatch.size());
    for (size_t i = 0; i < repetitiveBatch.size(); i++)
    {
        EXPECT_TRUE(LogEntriesEqual(repetitiveBatch[i], recoveredBatch[i]));
    }
}

// Test with a large batch of entries
TEST_F(CompressionTest, LargeBatch)
{
    std::vector<LogEntry> largeBatch(100, entry1);
    std::vector<uint8_t> serializedBatch = LogEntry::serializeBatch(std::move(largeBatch));
    std::vector<uint8_t> compressed = Compression::compress(std::move(serializedBatch));
    std::vector<uint8_t> decompressed = Compression::decompress(std::move(compressed));
    std::vector<LogEntry> recoveredBatch = LogEntry::deserializeBatch(std::move(decompressed));

    // Verify the correct number of entries
    ASSERT_EQ(largeBatch.size(), recoveredBatch.size());

    // Verify the entries match
    for (size_t i = 0; i < largeBatch.size(); i++)
    {
        EXPECT_TRUE(LogEntriesEqual(largeBatch[i], recoveredBatch[i]));
    }
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}