#include <gtest/gtest.h>
#include "Compression.hpp"
#include "Crypto.hpp"
#include "LogEntry.hpp"
#include <vector>
#include <string>
#include <memory>

class CompressionCryptoTest : public ::testing::Test
{
protected:
    Crypto crypto;

    void SetUp() override
    {
        // Create sample log entries for testing
        entry1 = LogEntry(LogEntry::ActionType::CREATE, "/data/records/1", "controller123", "processor123", "subject123");
        entry2 = LogEntry(LogEntry::ActionType::READ, "/data/records/2", "controller456", "processor456", "subject456");
        entry3 = LogEntry(LogEntry::ActionType::UPDATE, "/data/records/3", "controller789", "processor789", "subject789");

        // Create encryption key and IV
        key = std::vector<uint8_t>(32, 0x42);      // Fixed key for reproducibility
        wrongKey = std::vector<uint8_t>(32, 0x24); // Different key for testing
        dummyIV = std::vector<uint8_t>(12, 0x24);  // Fixed IV for reproducibility
    }

    // Helper function to compare two LogEntry objects
    bool LogEntriesEqual(const LogEntry &a, const LogEntry &b)
    {
        return a.serialize() == b.serialize();
    }

    LogEntry entry1, entry2, entry3;
    std::vector<uint8_t> key;
    std::vector<uint8_t> wrongKey;
    std::vector<uint8_t> dummyIV;
};

// Batch processing - original -> compress -> encrypt -> decrypt -> decompress -> recovered
TEST_F(CompressionCryptoTest, BatchProcessing)
{
    std::vector<LogEntry> batch = {entry1, entry2, entry3};
    std::vector<uint8_t> serializedBatch = LogEntry::serializeBatch(std::move(batch));
    std::vector<uint8_t> compressed = Compression::compress(std::move(serializedBatch));
    ASSERT_GT(compressed.size(), 0);

    std::vector<uint8_t> encrypted = crypto.encrypt(std::move(compressed), key, dummyIV);
    ASSERT_GT(encrypted.size(), 0);
    EXPECT_NE(encrypted, compressed);

    std::vector<uint8_t> decrypted = crypto.decrypt(encrypted, key, dummyIV);
    ASSERT_GT(decrypted.size(), 0);
    EXPECT_EQ(decrypted, compressed);

    std::vector<uint8_t> decompressed = Compression::decompress(std::move(decrypted));
    std::vector<LogEntry> recovered = LogEntry::deserializeBatch(std::move(decompressed));
    ASSERT_EQ(batch.size(), recovered.size());

    for (size_t i = 0; i < batch.size(); i++)
    {
        EXPECT_TRUE(LogEntriesEqual(batch[i], recovered[i]))
            << "Entries at index " << i << " don't match";
    }

    // Test with empty batch
    std::vector<LogEntry> emptyBatch;
    std::vector<uint8_t> emptySerializedBatch = LogEntry::serializeBatch(std::move(emptyBatch));
    std::vector<uint8_t> emptyCompressed = Compression::compress(std::move(emptySerializedBatch));
    std::vector<uint8_t> emptyEncrypted = crypto.encrypt(std::move(emptyCompressed), key, dummyIV);
    std::vector<uint8_t> emptyDecrypted = crypto.decrypt(emptyEncrypted, key, dummyIV);
    std::vector<uint8_t> emptyDecompressed = Compression::decompress(std::move(emptyDecrypted));
    std::vector<LogEntry> emptyRecovered = LogEntry::deserializeBatch(std::move(emptyDecompressed));
    EXPECT_TRUE(emptyRecovered.empty());

    // Test with single entry batch
    std::vector<LogEntry> singleBatch = {entry1};
    std::vector<uint8_t> singleSerializedBatch = LogEntry::serializeBatch(std::move(singleBatch));
    std::vector<uint8_t> singleCompressed = Compression::compress(std::move(singleSerializedBatch));
    std::vector<uint8_t> singleEncrypted = crypto.encrypt(std::move(singleCompressed), key, dummyIV);
    std::vector<uint8_t> singleDecrypted = crypto.decrypt(singleEncrypted, key, dummyIV);
    std::vector<uint8_t> singleDecompressed = Compression::decompress(std::move(singleDecrypted));
    std::vector<LogEntry> singleRecovered = LogEntry::deserializeBatch(std::move(singleDecompressed));
    ASSERT_EQ(1, singleRecovered.size());
    EXPECT_TRUE(LogEntriesEqual(entry1, singleRecovered[0]));
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}