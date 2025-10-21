#ifndef LOG_ENTRY_HPP
#define LOG_ENTRY_HPP

#include <string>
#include <chrono>
#include <vector>
#include <memory>
#include <cstdint>
#include <cstring>
#include <bitset>

constexpr int num_users = 128;

class LogEntry
{
public:
    enum class ActionType
    {
        CREATE = 0,
        READ = 1,
        UPDATE = 2,
        DELETE = 3,
    };

    LogEntry();

    // GDPRuler constructor
    LogEntry(uint64_t timestamp,
            std::string gdprKey,
            std::bitset<num_users> userKeyMap,
            uint8_t operationValidity,
            std::vector<uint8_t> payload = {});
    
    LogEntry(ActionType actionType,
             std::string dataLocation,
             std::string dataControllerId,
             std::string dataProcessorId,
             std::string dataSubjectId,
             std::vector<uint8_t> payload = std::vector<uint8_t>());

    // Serialization for GDPRuler
    std::vector<uint8_t> serializeGDPR() const;
    bool deserializeGDPR(const std::vector<uint8_t>& data);
    static std::vector<uint8_t> serializeBatchGDPR(std::vector<LogEntry> &&entries);
    static std::vector<LogEntry> deserializeBatchGDPR(std::vector<uint8_t> &&batchData);

    std::vector<uint8_t> serialize() &&;
    std::vector<uint8_t> serialize() const &;
    bool deserialize(std::vector<uint8_t> &&data);
    static std::vector<uint8_t> serializeBatch(std::vector<LogEntry> &&entries);
    static std::vector<LogEntry> deserializeBatch(std::vector<uint8_t> &&batchData);
    
    // GDPRuler Getters
    uint64_t getGDPRTimestamp() const { return m_gdpr_timestamp; }
    const std::string& getGDPRKey() const { return m_gdpr_key; }
    std::bitset<num_users> getUserKeyMap() const { return m_gdpr_user_key; }
    uint8_t getOperationValidity() const { return m_gdpr_operation_result; }
    const std::vector<uint8_t>& getNewValue() const { return m_gdpr_payload; }

    ActionType getActionType() const { return m_actionType; }
    std::string getDataLocation() const { return m_dataLocation; }
    std::string getDataControllerId() const { return m_dataControllerId; }
    std::string getDataProcessorId() const { return m_dataProcessorId; }
    std::string getDataSubjectId() const { return m_dataSubjectId; }
    std::chrono::system_clock::time_point getTimestamp() const { return m_timestamp; }
    const std::vector<uint8_t> &getPayload() const { return m_payload; }

private:
    // Helper methods for binary serialization
    void appendToVector(std::vector<uint8_t> &vec, const void *data, size_t size) const;
    void appendStringToVector(std::vector<uint8_t> &vec, const std::string &str) const;
    void appendStringToVector(std::vector<uint8_t> &vec, std::string &&str);
    bool extractFromVector(const std::vector<uint8_t>& vec, size_t& offset, void* data, size_t size) const;
    bool extractStringFromVector(std::vector<uint8_t> &vec, size_t &offset, std::string &str);

    ActionType m_actionType;                            // Type of GDPR operation
    std::string m_dataLocation;                         // Location of the data being operated on
    std::string m_dataControllerId;                     // ID of the entity controlling the data
    std::string m_dataProcessorId;                      // ID of the entity performing the operation
    std::string m_dataSubjectId;                        // ID of the data subject
    std::chrono::system_clock::time_point m_timestamp;  // When the operation occurred
    std::vector<uint8_t> m_payload;                     // optional extra bytes

    // GDPRuler fields
    uint64_t m_gdpr_timestamp;                          // 64-bit timestamp
    std::string m_gdpr_key;                             // key of logged operation
    std::bitset<num_users> m_gdpr_user_key;             // 128-bit user key bitmap
    uint8_t m_gdpr_operation_result;                    // Operation + validity bit
    std::vector<uint8_t> m_gdpr_payload;                // Arbitrary size new value
};

#endif