#pragma once
#include "SegmentedStorage.hpp"
#include "LogEntry.hpp"
#include "Crypto.hpp"
#include "Compression.hpp"
#include <memory>
#include <vector>
#include <string>
#include <chrono>
#include <filesystem>

class LogExporter {
public:
    LogExporter(std::shared_ptr<SegmentedStorage> storage, 
                bool useEncryption, 
                int compressionLevel);

    // Main export methods - similar to your old logger interface
    std::vector<std::string> exportLogsForKey(const std::string& key, 
                                             uint64_t timestampThreshold = std::numeric_limits<uint64_t>::max()) const;
    
    std::vector<std::string> exportAllLogs(uint64_t timestampThreshold = std::numeric_limits<uint64_t>::max()) const;
    
    std::vector<std::string> getLogFilesList() const;

    std::vector<std::string> getFilenames(const std::string& dir) const;

    // Export to file
    bool exportToFile(const std::string& outputPath,
                     std::chrono::system_clock::time_point fromTimestamp,
                     std::chrono::system_clock::time_point toTimestamp) const;

private:
    std::shared_ptr<SegmentedStorage> m_storage;
    bool m_useEncryption;
    int m_compressionLevel;
    
    // Crypto setup - similar to Writer class
    mutable Crypto m_crypto;
    std::vector<uint8_t> m_encryptionKey;
    std::vector<uint8_t> m_dummyIV;

    // Helper methods
    std::vector<std::string> readAndDecodeSegmentFile(const std::string& segmentFile, 
                                                     uint64_t timestampThreshold) const;
    std::string formatGDPRLogEntryReadable(const LogEntry& entry) const;
    std::string extractKeyFromFilename(const std::string& filename) const;
    std::string operationToString(uint8_t operation) const;
};
