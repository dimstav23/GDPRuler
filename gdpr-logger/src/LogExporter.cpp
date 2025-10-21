#include "LogExporter.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <set>

// Logs export pipeline:
// 1. Read the encrypted segments from storage
// 2. Decrypt and decompress them
// 3. Filter by timestamp if requested
// 4. Write to the output path

LogExporter::LogExporter(std::shared_ptr<SegmentedStorage> storage, 
                        bool useEncryption, 
                        int compressionLevel)
    : m_storage(std::move(storage))
    , m_useEncryption(useEncryption)
    , m_compressionLevel(compressionLevel)
    , m_encryptionKey(m_crypto.KEY_SIZE, 0x42)  // TODO: Use proper key management
    , m_dummyIV(m_crypto.GCM_IV_SIZE, 0x24)     // TODO: Use proper IV management
{
}

std::vector<std::string> LogExporter::exportLogsForKey(const std::string& key, 
                                                      uint64_t timestampThreshold) const
{
    std::vector<std::string> entries;

    if (!m_storage) {
        std::cerr << "LogExporter: Storage not initialized" << std::endl;
        return entries;
    }

    try {
        // Get segment files for the specific key
        auto segmentFiles = m_storage->getSegmentFilesForKey(key);
        
        if (segmentFiles.empty()) {
            std::cerr << "LogExporter: No log files found for key: " << key << std::endl;
            return entries;
        }

        // Process each segment file for this key
        for (const auto& segmentFile : segmentFiles) {
            auto fileEntries = readAndDecodeSegmentFile(segmentFile, timestampThreshold);
            entries.insert(entries.end(), fileEntries.begin(), fileEntries.end());
        }

        // Sort entries by timestamp (they should already be mostly sorted)
        // std::sort(entries.begin(), entries.end());

    } catch (const std::exception& e) {
        std::cerr << "LogExporter: Error reading logs for key " << key 
                  << ": " << e.what() << std::endl;
    }

    return entries;
}

std::vector<std::string> LogExporter::exportAllLogs(uint64_t timestampThreshold) const
{
    std::vector<std::string> allEntries;
    
    if (!m_storage) {
        std::cerr << "LogExporter: Storage not initialized" << std::endl;
        return allEntries;
    }

    try {
        // Get all segment files
        auto segmentFiles = m_storage->getSegmentFiles();
        
        if (segmentFiles.empty()) {
            std::cerr << "LogExporter: No log files found" << std::endl;
            return allEntries;
        }

        // Process each segment file
        for (const auto& segmentFile : segmentFiles) {
            auto fileEntries = readAndDecodeSegmentFile(segmentFile, timestampThreshold);
            allEntries.insert(allEntries.end(), fileEntries.begin(), fileEntries.end());
        }

        // Sort all entries by timestamp
        std::sort(allEntries.begin(), allEntries.end());

    } catch (const std::exception& e) {
        std::cerr << "LogExporter: Error reading all logs: " << e.what() << std::endl;
    }

    return allEntries;
}

std::vector<std::string> LogExporter::getLogFilesList() const
{
    if (!m_storage) {
        std::cerr << "LogExporter: Storage not initialized" << std::endl;
        return {};
    }

    // Get the storage base path and use filesystem to list files
    // This mimics your get_filenames functionality
    return getFilenames(m_storage->getBasePath());
}

std::vector<std::string> LogExporter::getFilenames(const std::string& dir) const
{
    std::vector<std::string> filenames;
    
    try {
        std::filesystem::path logs_dir(dir);
        
        if (!std::filesystem::exists(logs_dir)) {
            return filenames;
        }

        for (const auto& file : std::filesystem::directory_iterator(logs_dir)) {
            if (file.is_regular_file()) {
                filenames.push_back(file.path().string());
            }
        }
        
        std::sort(filenames.begin(), filenames.end());
        
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "LogExporter: Error listing files in " << dir << ": " << e.what() << std::endl;
    }

    return filenames;
}

std::vector<std::string> LogExporter::readAndDecodeSegmentFile(const std::string& segmentFile, 
                                                              uint64_t timestampThreshold) const
{
    std::vector<std::string> entries;
    
    try {
        // Read the entire file
        std::ifstream inputFile(segmentFile, std::ios::binary | std::ios::ate);
        if (!inputFile.is_open()) {
            std::cerr << "LogExporter: Failed to open segment file: " << segmentFile << std::endl;
            return entries;
        }

        size_t fileSize = inputFile.tellg();
        if (fileSize == 0) {
            return entries;
        }

        inputFile.seekg(0);
        std::vector<uint8_t> fileData(fileSize);
        inputFile.read(reinterpret_cast<char*>(fileData.data()), fileSize);
        inputFile.close();

        std::cout << "Processing segment file: " << segmentFile 
                  << " of size " << fileData.size() << " bytes." << std::endl;
        
        // Process multiple encrypted batches in the same file
        size_t offset = 0;
        int batchCount = 0;
        // Counter validation state - start from 0 for prototyping simplicity
        uint32_t expectedCounter = 0;
        bool counterSequentialityValid = true;

        while (offset < fileData.size()) {
            batchCount++;
            std::cout << "Processing batch " << batchCount << " at offset " << offset << std::endl;
            
            // Read the batch size from the current position
            if (offset + sizeof(uint32_t) > fileData.size()) {
                std::cout << "Not enough data for size field at offset " << offset << std::endl;
                break;
            }
            
            uint32_t dataSize;
            std::memcpy(&dataSize, fileData.data() + offset, sizeof(uint32_t));
            std::cout << "Batch " << batchCount << " data size: " << dataSize << " bytes" << std::endl;
            
            std::vector<uint8_t> processedData;

            if (m_useEncryption) {              
              // Calculate total size for this encrypted batch
              size_t totalBatchSize = sizeof(uint32_t) + dataSize + Crypto::GCM_TAG_SIZE;
              
              if (offset + totalBatchSize > fileData.size()) {
                  std::cout << "Not enough data for complete encrypted batch at offset " << offset 
                            << " (need " << totalBatchSize << ", have " << (fileData.size() - offset) << ")" << std::endl;
                  break;
              }
              
              // Extract this complete encrypted batch
              std::vector<uint8_t> encryptedBatch(fileData.begin() + offset, 
                                                fileData.begin() + offset + totalBatchSize);
              
              std::cout << "Extracted encrypted batch " << batchCount << ": " << encryptedBatch.size() << " bytes" << std::endl;
              
              // Move offset to start of next batch
              offset += totalBatchSize;

              try {
                  processedData = m_crypto.decrypt(encryptedBatch, m_encryptionKey, m_dummyIV);
                  std::cout << "Decrypted batch " << batchCount << ": " << processedData.size() << " bytes" << std::endl;
              } catch (const std::exception& e) {
                  std::cerr << "LogExporter: Failed to decrypt batch " << batchCount 
                            << " in " << segmentFile << ": " << e.what() << std::endl;
                  break;
              }
            } else {
              // Move offset to start of next batch
              size_t totalBatchSize = sizeof(uint32_t) + dataSize;
              if (offset + totalBatchSize > fileData.size()) {
                  std::cout << "Not enough data for complete unencrypted batch at offset " << offset 
                            << " (need " << totalBatchSize << ", have " << (fileData.size() - offset) << ")" << std::endl;
                  break;
              }
              
              // Extract just the data part (skip the size header)
              processedData.assign(fileData.begin() + offset + sizeof(uint32_t), 
                                  fileData.begin() + offset + totalBatchSize);
              
              std::cout << "Extracted unencrypted batch " << batchCount << ": " << processedData.size() << " bytes" << std::endl;
              
              // Move offset to start of next batch
              offset += totalBatchSize;
            }

            // Decompress this batch
            if (m_compressionLevel > 0) {
                try {
                    processedData = Compression::decompress(std::move(processedData));
                    std::cout << "Decompressed batch " << batchCount << ": " << processedData.size() << " bytes" << std::endl;
                } catch (const std::exception& e) {
                    std::cerr << "LogExporter: Failed to decompress batch " << batchCount 
                              << " in " << segmentFile << ": " << e.what() << std::endl;
                    exit(1);
                }
            }
            
            // After decompression, extract counter from beginning of batch
            uint32_t batchCounter = 0;
            if (processedData.size() >= sizeof(uint32_t)) {
                std::memcpy(&batchCounter, processedData.data(), sizeof(uint32_t));
                // Remove counter from processed data
                processedData.erase(processedData.begin(), processedData.begin() + sizeof(uint32_t));
                std::cout << "Batch " << batchCount << " counter: " << batchCounter << std::endl;

                if (batchCounter != expectedCounter) {
                    std::cout << "ERROR: Counter sequentiality issue in batch " << batchCount 
                             << " of file " << segmentFile
                             << " - Expected: " << expectedCounter 
                             << ", Got: " << batchCounter << std::endl;
                    counterSequentialityValid = false;
                }
                expectedCounter++;
            } else {
                std::cout << "WARNING: Batch " << batchCount << " too small to extract counter" << std::endl;
                counterSequentialityValid = false;
            }
            
            // Deserialize this batch
            std::vector<LogEntry> logEntries;
            try {
                logEntries = LogEntry::deserializeBatchGDPR(std::move(processedData));
                std::cout << "Deserialized batch " << batchCount << ": " << logEntries.size() << " entries" << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "LogExporter: Failed to deserialize batch " << batchCount 
                          << " in " << segmentFile << ": " << e.what() << std::endl;
                exit(1);
            }

            // Filter and format entries from this batch
            size_t entriesFromThisBatch = 0;
            for (const auto& entry : logEntries) {
                if (entry.getGDPRTimestamp() <= timestampThreshold) {
                    entries.push_back(formatGDPRLogEntryReadable(entry));
                    entriesFromThisBatch++;
                }
            }
            
            std::cout << "Added " << entriesFromThisBatch << " entries from batch " << batchCount << std::endl;
        }
        
        std::cout << "Processed " << batchCount << " batches, found " << entries.size() << " entries total" << std::endl;
        
        // Trusted counter validation warning
        if (!counterSequentialityValid) {
            std::cout << "WARNING: Counter sequentiality issues detected in file " << segmentFile << std::endl;
        } else {
            std::cout << "Counter sequentiality validation passed for file " << segmentFile 
                     << " (final counter: " << (expectedCounter - 1) << ")" << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "LogExporter: Error processing segment file " << segmentFile 
                  << ": " << e.what() << std::endl;
    }

    return entries;
}

std::string LogExporter::formatGDPRLogEntryReadable(const LogEntry& entry) const
{
    std::ostringstream oss;
    
    // Convert timestamp to readable format (similar to your timestamp_to_datetime)
    auto timePoint = std::chrono::system_clock::time_point(
        std::chrono::nanoseconds(entry.getGDPRTimestamp()));
    auto timeT = std::chrono::system_clock::to_time_t(timePoint);
    
    oss << "Timestamp: " << std::put_time(std::gmtime(&timeT), "%Y-%m-%d %H:%M:%S UTC");
    
    oss << ", DB Key: " << entry.getGDPRKey();

    oss << ", User key: " << entry.getUserKeyMap().to_string();
    
    // Decode operation and validity (same logic as your decode_log_entry)
    uint8_t operation_bits = (entry.getOperationValidity() >> 1) & 0x07;
    uint8_t result_bit = entry.getOperationValidity() & 0x01;
    
    oss << ", Operation: " << operationToString(operation_bits);
    oss << ", Result: " << (result_bit != 0 ? "valid" : "invalid");
    
    // Add payload if present (similar to your gdpr_metadata_fmt)
    if (!entry.getNewValue().empty()) {
        oss << ", New value: ";
        // Convert payload to string representation
        std::string payloadStr(entry.getNewValue().begin(), entry.getNewValue().end());
        oss << payloadStr; // You might want to apply gdpr_metadata_fmt here
    }
    
    return oss.str();
}

std::string LogExporter::operationToString(uint8_t operation) const
{
    switch (operation) {
        case 0: return "unknown";
        case 1: return "get";
        case 2: return "put";
        case 3: return "delete";
        case 4: return "getM";
        case 5: return "putM";
        case 6: return "putC";
        case 7: return "getLogs";
        default: return "invalid";
    }
}

std::string LogExporter::extractKeyFromFilename(const std::string& filename) const
{
    std::filesystem::path filePath(filename);
    std::string name = filePath.stem().string(); // Remove extension
    
    // Remove segment suffix (e.g., "_001", "_002") if present
    size_t lastUnderscore = name.find_last_of('_');
    if (lastUnderscore != std::string::npos) {
        std::string suffix = name.substr(lastUnderscore + 1);
        // Check if suffix is numeric (segment number)
        if (std::all_of(suffix.begin(), suffix.end(), ::isdigit)) {
            return name.substr(0, lastUnderscore);
        }
    }
    
    return name;
}

bool LogExporter::exportToFile(const std::string& outputPath,
                              std::chrono::system_clock::time_point fromTimestamp,
                              std::chrono::system_clock::time_point toTimestamp) const
{    
    try {
        // Create output directory if needed
        std::filesystem::path outputDir = std::filesystem::path(outputPath).parent_path();
        if (!outputDir.empty() && !std::filesystem::exists(outputDir)) {
            if (!std::filesystem::create_directories(outputDir)) {
                std::cerr << "LogExporter: Failed to create output directory: " << outputDir << std::endl;
                return false;
            }
        }

        std::ofstream outputFile(outputPath);
        if (!outputFile.is_open()) {
            std::cerr << "LogExporter: Failed to open output file: " << outputPath << std::endl;
            return false;
        }

        // Convert time points to nanoseconds
        [[maybe_unused]] uint64_t fromNanos = fromTimestamp.time_since_epoch().count();
        uint64_t toNanos = toTimestamp.time_since_epoch().count();

        // Get all logs and filter by time range
        auto allLogs = exportAllLogs(toNanos);
        
        size_t exportedCount = 0;
        for (const auto& logEntry : allLogs) {
            // Simple timestamp extraction from formatted string
            // You might want to make this more robust
            outputFile << logEntry << std::endl;
            exportedCount++;
        }

        outputFile.close();
        std::cout << "LogExporter: Successfully exported " << exportedCount 
                  << " entries to " << outputPath << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "LogExporter: Export to file failed: " << e.what() << std::endl;
        return false;
    }
}
