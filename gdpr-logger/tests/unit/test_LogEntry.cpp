#include <gtest/gtest.h>
#include "LogEntry.hpp"
#include <vector>
#include <iostream>
#include <chrono>

// Test default constructor
TEST(LogEntryTest1, DefaultConstructor_InitializesCorrectly)
{
    LogEntry entry;

    EXPECT_EQ(entry.getActionType(), LogEntry::ActionType::CREATE);
    EXPECT_EQ(entry.getDataLocation(), "");
    EXPECT_EQ(entry.getDataControllerId(), "");
    EXPECT_EQ(entry.getDataProcessorId(), "");
    EXPECT_EQ(entry.getDataSubjectId(), "");
    EXPECT_EQ(entry.getPayload().size(), 0);

    auto now = std::chrono::system_clock::now();
    EXPECT_NEAR(std::chrono::system_clock::to_time_t(entry.getTimestamp()),
                std::chrono::system_clock::to_time_t(now), 1);
}

// Test parameterized constructor
TEST(LogEntryTest2, ParameterizedConstructor_SetsFieldsCorrectly)
{
    std::vector<uint8_t> testPayload(128, 0xAA); // 128 bytes of 0xAA
    LogEntry entry(LogEntry::ActionType::UPDATE, "database/users", "controller123", "processor789", "subject456", testPayload);

    EXPECT_EQ(entry.getActionType(), LogEntry::ActionType::UPDATE);
    EXPECT_EQ(entry.getDataLocation(), "database/users");
    EXPECT_EQ(entry.getDataControllerId(), "controller123");
    EXPECT_EQ(entry.getDataProcessorId(), "processor789");
    EXPECT_EQ(entry.getDataSubjectId(), "subject456");
    EXPECT_EQ(entry.getPayload().size(), testPayload.size());

    // Check content matches
    const auto &payload = entry.getPayload();
    bool contentMatches = true;
    for (size_t i = 0; i < payload.size(); ++i)
    {
        if (payload[i] != testPayload[i])
        {
            contentMatches = false;
            break;
        }
    }
    EXPECT_TRUE(contentMatches);

    auto now = std::chrono::system_clock::now();
    EXPECT_NEAR(std::chrono::system_clock::to_time_t(entry.getTimestamp()),
                std::chrono::system_clock::to_time_t(now), 1);
}

// Test GDPR constructor
TEST(LogEntryTest3, GDPRConstructor_SetsFieldsCorrectly)
{
    std::bitset<128> userKeyMap;
    userKeyMap.set(5);  // Set bit 5
    userKeyMap.set(64); // Set bit 64
    userKeyMap.set(127); // Set bit 127
    
    std::vector<uint8_t> newValue = {0x01, 0x02, 0x03, 0xFF, 0xAB};
    uint64_t timestamp = 1234567890123456ULL;
    std::string gdprKey = "user123";
    uint8_t operationValidity = (2 << 1) | 1; // UPDATE operation, valid=true

    LogEntry entry(timestamp, gdprKey, userKeyMap, operationValidity, newValue);

    EXPECT_EQ(entry.getGDPRTimestamp(), timestamp);
    EXPECT_EQ(entry.getGDPRKey(), gdprKey);
    EXPECT_EQ(entry.getUserKeyMap(), userKeyMap);
    EXPECT_EQ(entry.getOperationValidity(), operationValidity);
    EXPECT_EQ(entry.getNewValue(), newValue);
}

// Test serialization and deserialization with empty payload
TEST(LogEntryTest4, SerializationDeserialization_WorksCorrectly)
{
    LogEntry entry(LogEntry::ActionType::READ, "storage/files", "controllerABC", "processorDEF", "subjectXYZ");

    std::vector<uint8_t> serializedData = entry.serialize();
    LogEntry newEntry;
    bool success = newEntry.deserialize(std::move(serializedData));

    EXPECT_TRUE(success);
    EXPECT_EQ(newEntry.getActionType(), LogEntry::ActionType::READ);
    EXPECT_EQ(newEntry.getDataLocation(), "storage/files");
    EXPECT_EQ(newEntry.getDataControllerId(), "controllerABC");
    EXPECT_EQ(newEntry.getDataProcessorId(), "processorDEF");
    EXPECT_EQ(newEntry.getDataSubjectId(), "subjectXYZ");
    EXPECT_EQ(newEntry.getPayload().size(), 0); // Payload should still be empty

    std::vector<uint8_t> serializedData2 = entry.serialize();
    success = newEntry.deserialize(std::move(serializedData2));

    EXPECT_TRUE(success);

    EXPECT_EQ(newEntry.getActionType(), LogEntry::ActionType::READ);
    EXPECT_EQ(newEntry.getDataLocation(), "storage/files");
    EXPECT_EQ(newEntry.getDataControllerId(), "controllerABC");
    EXPECT_EQ(newEntry.getDataSubjectId(), "subjectXYZ");
    EXPECT_NEAR(std::chrono::system_clock::to_time_t(newEntry.getTimestamp()),
                std::chrono::system_clock::to_time_t(entry.getTimestamp()), 1);
}

// Test serialization and deserialization with payload
TEST(LogEntryTest4A, SerializationDeserializationWithPayload_WorksCorrectly)
{
    // Create test payload
    std::vector<uint8_t> testPayload(64);
    for (size_t i = 0; i < testPayload.size(); ++i)
    {
        testPayload[i] = static_cast<uint8_t>(i & 0xFF);
    }

    LogEntry entry(LogEntry::ActionType::READ, "storage/files", "controllerABC", "processorDEF", "subjectXYZ", testPayload);

    // Serialize and deserialize
    std::vector<uint8_t> serializedData = entry.serialize();
    LogEntry newEntry;
    bool success = newEntry.deserialize(std::move(serializedData));

    // Verify deserialization worked
    EXPECT_TRUE(success);
    EXPECT_EQ(newEntry.getActionType(), LogEntry::ActionType::READ);
    EXPECT_EQ(newEntry.getDataLocation(), "storage/files");
    EXPECT_EQ(newEntry.getDataControllerId(), "controllerABC");
    EXPECT_EQ(newEntry.getDataProcessorId(), "processorDEF");
    EXPECT_EQ(newEntry.getDataSubjectId(), "subjectXYZ");

    // Verify payload
    EXPECT_EQ(newEntry.getPayload().size(), testPayload.size());

    // Check payload content
    const auto &recoveredPayload = newEntry.getPayload();
    bool payloadMatches = true;
    for (size_t i = 0; i < testPayload.size(); ++i)
    {
        if (recoveredPayload[i] != testPayload[i])
        {
            payloadMatches = false;
            break;
        }
    }
    EXPECT_TRUE(payloadMatches);
}

// Test batch serialization and deserialization with payloads
TEST(LogEntryTest5, BatchSerializationDeserialization_WorksCorrectly)
{
    // Create a batch of log entries
    std::vector<LogEntry> originalEntries;

    // Entry with no payload
    originalEntries.push_back(LogEntry(LogEntry::ActionType::CREATE, "db/users", "controller1", "processor1", "subject1"));

    // Entry with small payload
    std::vector<uint8_t> payload2(16, 0x22); // 16 bytes of 0x22
    originalEntries.push_back(LogEntry(LogEntry::ActionType::READ, "files/documents", "controller2", "processor2", "subject2", payload2));

    // Entry with medium payload
    std::vector<uint8_t> payload3(128, 0x33); // 128 bytes of 0x33
    originalEntries.push_back(LogEntry(LogEntry::ActionType::UPDATE, "cache/profiles", "controller3", "processor3", "subject3", payload3));

    // Entry with large payload
    std::vector<uint8_t> payload4(1024, 0x44); // 1024 bytes of 0x44
    originalEntries.push_back(LogEntry(LogEntry::ActionType::DELETE, "archive/logs", "controller4", "processor4", "subject4", payload4));

    // Serialize the batch
    std::vector<uint8_t> batchData = LogEntry::serializeBatch(std::move(originalEntries));

    // Check that the batch has reasonable size
    EXPECT_GT(batchData.size(), sizeof(uint32_t)); // At least space for entry count

    // Deserialize the batch
    std::vector<LogEntry> recoveredEntries = LogEntry::deserializeBatch(std::move(batchData));

    // Verify the number of entries
    EXPECT_EQ(recoveredEntries.size(), originalEntries.size());

    // Verify each entry's data
    for (size_t i = 0; i < originalEntries.size() && i < recoveredEntries.size(); ++i)
    {
        EXPECT_EQ(recoveredEntries[i].getActionType(), originalEntries[i].getActionType());
        EXPECT_EQ(recoveredEntries[i].getDataLocation(), originalEntries[i].getDataLocation());
        EXPECT_EQ(recoveredEntries[i].getDataControllerId(), originalEntries[i].getDataControllerId());
        EXPECT_EQ(recoveredEntries[i].getDataProcessorId(), originalEntries[i].getDataProcessorId());
        EXPECT_EQ(recoveredEntries[i].getDataSubjectId(), originalEntries[i].getDataSubjectId());

        // Verify payload size
        EXPECT_EQ(recoveredEntries[i].getPayload().size(), originalEntries[i].getPayload().size());

        // Check payload content if it's not empty
        if (!originalEntries[i].getPayload().empty())
        {
            const auto &originalPayload = originalEntries[i].getPayload();
            const auto &recoveredPayload = recoveredEntries[i].getPayload();

            bool payloadMatches = true;
            for (size_t j = 0; j < originalPayload.size(); ++j)
            {
                if (recoveredPayload[j] != originalPayload[j])
                {
                    payloadMatches = false;
                    break;
                }
            }
            EXPECT_TRUE(payloadMatches) << "Payload mismatch at entry " << i;
        }

        // Compare timestamps (allowing 1 second difference for potential precision issues)
        EXPECT_NEAR(
            std::chrono::system_clock::to_time_t(recoveredEntries[i].getTimestamp()),
            std::chrono::system_clock::to_time_t(originalEntries[i].getTimestamp()),
            1);
    }
}

// ==============================================
// NEW GDPR-SPECIFIC TESTS
// ==============================================

// Test GDPR serialization and deserialization
TEST(LogEntryGDPRTest, SerializationDeserialization_WorksCorrectly)
{
    std::bitset<128> userKeyMap;
    userKeyMap.set(3);   // Set some specific bits
    userKeyMap.set(15);
    userKeyMap.set(64);
    userKeyMap.set(127);

    std::vector<uint8_t> newValue(50, 0xAB);
    uint64_t timestamp = 1234567890123456ULL;
    std::string gdprKey = "test_user_key";
    uint8_t operationValidity = (2 << 1) | 1; // UPDATE operation, valid=true

    LogEntry entry(timestamp, gdprKey, userKeyMap, operationValidity, newValue);

    std::vector<uint8_t> serializedData = entry.serializeGDPR();
    LogEntry deserializedEntry;
    bool success = deserializedEntry.deserializeGDPR(serializedData);

    EXPECT_TRUE(success);
    EXPECT_EQ(deserializedEntry.getGDPRTimestamp(), timestamp);
    EXPECT_EQ(deserializedEntry.getGDPRKey(), gdprKey);
    EXPECT_EQ(deserializedEntry.getUserKeyMap(), userKeyMap);
    EXPECT_EQ(deserializedEntry.getOperationValidity(), operationValidity);
    EXPECT_EQ(deserializedEntry.getNewValue(), newValue);
}

// Test GDPR batch serialization and deserialization
TEST(LogEntryGDPRTest, BatchSerializationDeserialization_WorksCorrectly)
{
    std::vector<LogEntry> originalEntries;

    // Entry 1: GET operation, valid
    std::bitset<128> userMap1;
    userMap1.set(1);
    userMap1.set(10);
    std::vector<uint8_t> payload1 = {0x01, 0x02, 0x03};
    originalEntries.emplace_back(1000, "key1", userMap1, (1 << 1) | 1, payload1);

    // Entry 2: DELETE operation, invalid
    std::bitset<128> userMap2;
    userMap2.set(50);
    userMap2.set(100);
    std::vector<uint8_t> payload2 = {0x04, 0x05, 0x06, 0x07};
    originalEntries.emplace_back(2000, "key2", userMap2, (3 << 1) | 0, payload2);

    // Entry 3: PUT operation with large payload, valid
    std::bitset<128> userMap3;
    userMap3.set(0);
    userMap3.set(127);
    std::vector<uint8_t> payload3(256, 0xFF);
    originalEntries.emplace_back(3000, "key3", userMap3, (2 << 1) | 1, payload3);

    // Serialize batch using GDPR format
    std::vector<uint8_t> batchData = LogEntry::serializeBatchGDPR(std::move(originalEntries));
    EXPECT_GT(batchData.size(), sizeof(uint32_t));

    // Deserialize batch
    std::vector<LogEntry> recoveredEntries = LogEntry::deserializeBatchGDPR(std::move(batchData));
    EXPECT_EQ(recoveredEntries.size(), 3);

    // Verify entry 1
    EXPECT_EQ(recoveredEntries[0].getGDPRTimestamp(), 1000);
    EXPECT_EQ(recoveredEntries[0].getGDPRKey(), "key1");
    EXPECT_EQ(recoveredEntries[0].getUserKeyMap(), userMap1);
    EXPECT_EQ(recoveredEntries[0].getOperationValidity(), (1 << 1) | 1);
    EXPECT_EQ(recoveredEntries[0].getNewValue(), payload1);

    // Verify entry 2
    EXPECT_EQ(recoveredEntries[1].getGDPRTimestamp(), 2000);
    EXPECT_EQ(recoveredEntries[1].getGDPRKey(), "key2");
    EXPECT_EQ(recoveredEntries[1].getUserKeyMap(), userMap2);
    EXPECT_EQ(recoveredEntries[1].getOperationValidity(), (3 << 1) | 0);
    EXPECT_EQ(recoveredEntries[1].getNewValue(), payload2);

    // Verify entry 3
    EXPECT_EQ(recoveredEntries[2].getGDPRTimestamp(), 3000);
    EXPECT_EQ(recoveredEntries[2].getGDPRKey(), "key3");
    EXPECT_EQ(recoveredEntries[2].getUserKeyMap(), userMap3);
    EXPECT_EQ(recoveredEntries[2].getOperationValidity(), (2 << 1) | 1);
    EXPECT_EQ(recoveredEntries[2].getNewValue(), payload3);
}

// Test GDPR serialization with empty payload
TEST(LogEntryGDPRTest, SerializationWithEmptyPayload_WorksCorrectly)
{
    std::bitset<128> userKeyMap;
    userKeyMap.set(42);

    uint64_t timestamp = 9876543210ULL;
    std::string gdprKey = "empty_payload_key";
    uint8_t operationValidity = (1 << 1) | 0; // GET operation, invalid
    std::vector<uint8_t> emptyPayload; // Empty payload

    LogEntry entry(timestamp, gdprKey, userKeyMap, operationValidity, emptyPayload);

    std::vector<uint8_t> serializedData = entry.serializeGDPR();
    LogEntry deserializedEntry;
    bool success = deserializedEntry.deserializeGDPR(serializedData);

    EXPECT_TRUE(success);
    EXPECT_EQ(deserializedEntry.getGDPRTimestamp(), timestamp);
    EXPECT_EQ(deserializedEntry.getGDPRKey(), gdprKey);
    EXPECT_EQ(deserializedEntry.getUserKeyMap(), userKeyMap);
    EXPECT_EQ(deserializedEntry.getOperationValidity(), operationValidity);
    EXPECT_TRUE(deserializedEntry.getNewValue().empty());
}

// Test GDPR serialization with maximum user key map
TEST(LogEntryGDPRTest, SerializationWithMaxUserKeyMap_WorksCorrectly)
{
    std::bitset<128> userKeyMap;
    // Set all bits to 1
    userKeyMap.flip(); // Sets all bits to 1

    uint64_t timestamp = UINT64_MAX;
    std::string gdprKey = "max_test_key_with_very_long_name_to_test_string_handling";
    uint8_t operationValidity = 0xFF; // All bits set
    std::vector<uint8_t> payload(1000, 0x55);

    LogEntry entry(timestamp, gdprKey, userKeyMap, operationValidity, payload);

    std::vector<uint8_t> serializedData = entry.serializeGDPR();
    LogEntry deserializedEntry;
    bool success = deserializedEntry.deserializeGDPR(serializedData);

    EXPECT_TRUE(success);
    EXPECT_EQ(deserializedEntry.getGDPRTimestamp(), timestamp);
    EXPECT_EQ(deserializedEntry.getGDPRKey(), gdprKey);
    EXPECT_EQ(deserializedEntry.getUserKeyMap(), userKeyMap);
    EXPECT_EQ(deserializedEntry.getOperationValidity(), operationValidity);
    EXPECT_EQ(deserializedEntry.getNewValue(), payload);
}

// Test operation validity bit extraction
TEST(LogEntryGDPRTest, OperationValidityBitExtraction_WorksCorrectly)
{
    std::bitset<128> userKeyMap;
    std::vector<uint8_t> payload;

    // Test different operation/validity combinations
    struct TestCase {
        uint8_t operation;
        bool valid;
        uint8_t expected_encoded;
        std::string testKey;
    };

    std::vector<TestCase> testCases = {
        {0, false, (0 << 1) | 0, "key_op0_invalid"},
        {0, true,  (0 << 1) | 1, "key_op0_valid"},  
        {1, false, (1 << 1) | 0, "key_op1_invalid"},
        {1, true,  (1 << 1) | 1, "key_op1_valid"},
        {2, false, (2 << 1) | 0, "key_op2_invalid"},
        {2, true,  (2 << 1) | 1, "key_op2_valid"},
        {3, false, (3 << 1) | 0, "key_op3_invalid"},
        {3, true,  (3 << 1) | 1, "key_op3_valid"},
        {7, true,  (7 << 1) | 1, "key_op7_valid"},
    };

    for (const auto& testCase : testCases) {
        LogEntry entry(1000, testCase.testKey, userKeyMap, testCase.expected_encoded, payload);

        std::vector<uint8_t> serializedData = entry.serializeGDPR();
        LogEntry deserializedEntry;
        bool success = deserializedEntry.deserializeGDPR(serializedData);

        EXPECT_TRUE(success);
        EXPECT_EQ(deserializedEntry.getGDPRKey(), testCase.testKey);
        EXPECT_EQ(deserializedEntry.getOperationValidity(), testCase.expected_encoded)
            << "Failed for operation=" << static_cast<int>(testCase.operation) 
            << ", valid=" << testCase.valid;

        // Extract operation and validity
        uint8_t extractedOperation = (deserializedEntry.getOperationValidity() >> 1) & 0x07;
        bool extractedValid = (deserializedEntry.getOperationValidity() & 0x01) != 0;
        
        EXPECT_EQ(extractedOperation, testCase.operation);
        EXPECT_EQ(extractedValid, testCase.valid);
    }
}