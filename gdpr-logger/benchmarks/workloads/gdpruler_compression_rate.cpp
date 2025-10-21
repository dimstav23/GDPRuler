#include "BenchmarkUtils.hpp"
#include "LoggingManager.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <future>
#include <optional>
#include <filesystem>
#include <numeric>
#include <fstream>
#include <random>
#include <bitset>
#include <iomanip>
#include <sstream>
#include <sys/resource.h>

// LogFileHasher class for consistent key-to-filename mapping
class LogFileHasher {
private:
  static inline size_t m_max_files = 512;

  struct string_hash {
    using hash_type = std::hash<std::string_view>;
    using is_transparent = void;
    
    size_t operator()(const char* str) const { return hash_type{}(str); }
    size_t operator()(std::string_view str) const { return hash_type{}(str); }
    size_t operator()(const std::string& str) const { return hash_type{}(str); }
  };
    
public:
  // Set the max files (to be used during initialization)
  static void set_max_files(size_t max_files) {
    m_max_files = max_files;
  }
  
  // Filename generation using the hash of the key for better distribution
  static std::string hash_key_to_filename(std::string_view key) {
    string_hash hasher;
    size_t hash_value = hasher(key) % m_max_files;
    return std::to_string(hash_value);
  }
  
  // Get current max files setting
  static size_t get_max_files() {
    return m_max_files;
  }
};

// Zipfian distribution for realistic key access patterns
class ZipfianGenerator {
private:
    std::vector<double> probabilities;
    std::discrete_distribution<int> distribution;
    std::mt19937 generator;
    
public:
    ZipfianGenerator(int n, double theta, unsigned seed = std::random_device{}()) 
        : generator(seed) {
        probabilities.resize(n);
        
        // Calculate Zipfian probabilities
        double sum = 0.0;
        for (int i = 1; i <= n; ++i) {
            sum += 1.0 / std::pow(i, theta);
        }
        
        for (int i = 0; i < n; ++i) {
            probabilities[i] = (1.0 / std::pow(i + 1, theta)) / sum;
        }
        
        distribution = std::discrete_distribution<int>(probabilities.begin(), probabilities.end());
    }
    
    int next() {
        return distribution(generator);
    }
};

// Configuration for compression benchmark
struct CompressionConfig {
    int entrySize;            // Entry sizes: 1024, 4096 bytes
    bool useEncryption;       // Encryption on/off
    int compressionLevel;     // Compression levels: 0, 3, 6, 9
    int numProducers;         // Number of producer threads: 32
    int entriesPerProducer;   // Entries per producer
    double zipfianTheta;      // Zipfian distribution parameter
    int numKeys;              // Total number of unique keys
};

// Results for compression benchmark run
struct CompressionResult {
    CompressionConfig config;
    double executionTimeSeconds;
    size_t totalEntries;
    double avgEntrySize;
    double totalDataSizeGiB;
    double finalStorageSizeGiB;
    double compressionRatio;          // original_size / compressed_size
    double compressionReduction;      // (1 - compressed_size / original_size) * 100%
    double entriesThroughput;
    double logicalThroughputGiB;
    double physicalThroughputGiB;
};

// Improved directory cleanup and creation
void setupBenchmarkDirectory(const std::string& path) {
    try {
        // Remove directory if it exists
        if (std::filesystem::exists(path)) {
            std::filesystem::remove_all(path);
            std::cout << "Removed existing directory: " << path << std::endl;
        }
        
        // Create fresh directory
        if (!std::filesystem::create_directories(path)) {
            throw std::runtime_error("Failed to create directory: " + path);
        }
        
        // Verify directory was created and is writable
        if (!std::filesystem::exists(path)) {
            throw std::runtime_error("Directory creation failed: " + path);
        }
        
        if (!std::filesystem::is_directory(path)) {
            throw std::runtime_error("Path exists but is not a directory: " + path);
        }
        
        std::cout << "Created benchmark directory: " << path << std::endl;
        
    } catch (const std::filesystem::filesystem_error& e) {
        throw std::runtime_error("Filesystem error: " + std::string(e.what()));
    }
}

// Generate realistic GDPR keys in the format: user123456789123456789 or key123456789123456789
std::string generateGDPRKey(int keyIndex) {
    // The keyIndex is already Zipfian distributed, so use it deterministically
    // to create a realistic-looking large number
    uint64_t baseNumber = 100000000000000000ULL; // Start of 18-digit range
    uint64_t keyRange = 899999999999999999ULL;   // Range size for 18 digits
    
    // Create deterministic but realistic number from keyIndex
    uint64_t keyNumber = baseNumber + (static_cast<uint64_t>(keyIndex) * 7919ULL) % keyRange;
    
    std::stringstream ss;
    // Alternate between "user" and "key" prefixes based on keyIndex
    if (keyIndex % 2 == 0) {
        ss << "user" << keyNumber;
    } else {
        ss << "key" << keyNumber;
    }
    
    return ss.str();
}

// Generate realistic user key bitmap with 1 bit set
std::bitset<128> generateUserKeyMap() {
    static std::mt19937 gen(std::random_device{}());
    static std::uniform_int_distribution<> dis(0, 127);
    
    std::bitset<128> userMap;
    
    // Always set exactly 1 random bit
    userMap.set(dis(gen));
    
    return userMap;
}

// Generate realistic payload data (semi-friendly to compression but not optimal)
std::vector<uint8_t> generateRealisticPayload(int targetSize, int keyIndex) {
    // Account for fixed overhead: timestamp(8) + key_size(4) + user_key(16) + operation(1) + payload_size(4) = 33 bytes
    static const int FIXED_OVERHEAD = 33;
    
    // Account for key size: "user" or "key" + 18 digits = 22 bytes  
    static const int KEY_SIZE = 22;
    
    size_t payloadSize = std::max(1, targetSize - FIXED_OVERHEAD - KEY_SIZE);
    
    std::vector<uint8_t> payload(payloadSize);
    
    // Create semi-realistic payload that mimics database values/JSON/XML data
    static std::mt19937 gen(std::random_device{}());
    static std::uniform_int_distribution<uint8_t> charDis(32, 126); // Printable ASCII
    static std::uniform_int_distribution<int> structureDis(0, 100);
    static std::uniform_int_distribution<int> lengthDis(3, 15);
    
    // Common patterns in real data
    std::vector<std::string> commonStrings = {
        "null", "true", "false", "user", "admin", "guest", "data", "value", "name",
        "email", "address", "phone", "status", "active", "inactive", "pending",
        "json", "xml", "http", "https", "www", "com", "org", "net", "error",
        "success", "failure", "timeout", "connection", "database", "table", "field"
    };
    
    std::vector<std::string> numbers = {
        "0", "1", "10", "100", "1000", "999", "404", "200", "500", "201", "301"
    };
    
    size_t pos = 0;
    while (pos < payloadSize) {
        int chance = structureDis(gen);
        
        if (chance < 20 && pos < payloadSize - 10) {
            // Add a common string (20% chance)
            const auto& str = commonStrings[keyIndex % commonStrings.size()];
            size_t len = std::min(str.length(), payloadSize - pos);
            std::memcpy(&payload[pos], str.c_str(), len);
            pos += len;
        } else if (chance < 30 && pos < payloadSize - 5) {
            // Add a number (10% chance)
            const auto& num = numbers[keyIndex % numbers.size()];
            size_t len = std::min(num.length(), payloadSize - pos);
            std::memcpy(&payload[pos], num.c_str(), len);
            pos += len;
        } else if (chance < 35) {
            // Add structure chars like {, }, [, ], :, , (5% chance)
            char structChars[] = {'{', '}', '[', ']', ':', ',', '"', '='};
            payload[pos] = structChars[keyIndex % sizeof(structChars)];
            pos++;
        } else if (chance < 45) {
            // Add whitespace/newlines (10% chance)
            char whitespace[] = {' ', ' ', ' ', '\t', '\n'};
            payload[pos] = whitespace[keyIndex % sizeof(whitespace)];
            pos++;
        } else {
            // Add random printable character (55% chance)
            payload[pos] = charDis(gen);
            pos++;
        }
    }
    
    return payload;
}

// Generate individual entries for compression testing
std::vector<std::pair<LogEntry, std::string>> generateGDPREntriesForCompression(const CompressionConfig& config) {
    std::vector<std::pair<LogEntry, std::string>> entries;
    ZipfianGenerator zipfGen(config.numKeys, config.zipfianTheta);
    
    int totalEntries = config.numProducers * config.entriesPerProducer;
    entries.reserve(totalEntries);
    
    std::cout << "Generating " << totalEntries << " GDPR entries for compression testing..." << std::flush;
    
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
        now.time_since_epoch()).count();
    
    for (int entryIdx = 0; entryIdx < totalEntries; ++entryIdx) {
        int keyIndex = zipfGen.next();
        
        std::string gdprKey = generateGDPRKey(keyIndex);
        auto userKeyMap = generateUserKeyMap();
        auto payload = generateRealisticPayload(config.entrySize, keyIndex);
        
        // Realistic operation validity (3 bits operation + 1 bit validity)
        uint8_t operationValidity = ((keyIndex % 7 + 1) << 1) | (keyIndex % 2);
        
        LogEntry entry(timestamp + entryIdx,
                      gdprKey,
                      userKeyMap,
                      operationValidity,
                      std::move(payload));
        
        // Hash the key to get filename
        std::string filename = LogFileHasher::hash_key_to_filename(gdprKey);
        
        entries.emplace_back(std::move(entry), std::move(filename));
    }
    
    std::cout << " Done." << std::endl;
    return entries;
}

// Append individual entries for compression testing
LatencyCollector appendGDPREntriesForCompression(LoggingManager &loggingManager, 
                                                const std::vector<std::pair<LogEntry, std::string>>& entries,
                                                int startIndex, int numEntries) {
    LatencyCollector localCollector;
    localCollector.reserve(numEntries);

    auto token = loggingManager.createProducerToken();

    for (int i = 0; i < numEntries; ++i) {
        const auto& [entry, filename] = entries[startIndex + i];
        
        // Create a copy of the entry since LoggingManager takes ownership
        LogEntry entryCopy(entry.getGDPRTimestamp(),
                          entry.getGDPRKey(),
                          entry.getUserKeyMap(),
                          entry.getOperationValidity(),
                          entry.getNewValue());
        
        // Measure latency for each append call
        auto startTime = std::chrono::high_resolution_clock::now();

        bool success = loggingManager.append(std::move(entryCopy), token, filename);

        auto endTime = std::chrono::high_resolution_clock::now();
        auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - startTime);

        // Record the latency measurement
        localCollector.addMeasurement(latency);

        if (!success) {
            std::cerr << "Failed to append GDPR entry to " << filename << std::endl;
        }
    }

    return localCollector;
}

// Run compression benchmark configuration
CompressionResult runCompressionBenchmark(const CompressionConfig& config, 
                                         const std::vector<std::pair<LogEntry, std::string>>& entries) {
    std::cout << "\n=========================================" << std::endl;
    std::cout << "Running compression benchmark: " 
              << config.entrySize << " byte entries, "
              << "encryption=" << (config.useEncryption ? "ON" : "OFF") << ", "
              << "compression=" << config.compressionLevel << std::endl;
    std::cout << "=========================================" << std::endl;
    
    // Setup logging configuration with fixed parameters
    LoggingConfig loggingConfig;
    loggingConfig.basePath = "/scratch/dimitrios/gdpruler_fs/compression_benchmark_logs";
    loggingConfig.baseFilename = "gdpr_compression";
    loggingConfig.maxSegmentSize = 100 * 1024 * 1024; // 100 MB per file
    loggingConfig.numWriterThreads = 4;
    loggingConfig.batchSize = 8192;
    loggingConfig.queueCapacity = 2 * loggingConfig.numWriterThreads * loggingConfig.batchSize;
    loggingConfig.maxExplicitProducers = 16;
    loggingConfig.maxAttempts = 5;
    loggingConfig.baseRetryDelay = std::chrono::milliseconds(1);
    loggingConfig.appendTimeout = std::chrono::minutes(5);
    loggingConfig.useEncryption = config.useEncryption;
    loggingConfig.compressionLevel = config.compressionLevel;
    
    // Configure LogFileHasher and set reasonable maxOpenFiles
    struct rlimit limits{};
    if (getrlimit(RLIMIT_NOFILE, &limits) == 0) {
        LogFileHasher::set_max_files(static_cast<int>(0.9 * limits.rlim_cur));
    }
    loggingConfig.maxOpenFiles = LogFileHasher::get_max_files();
    
    // Setup benchmark directory
    setupBenchmarkDirectory(loggingConfig.basePath);
    
    // Calculate expected data size
    size_t totalEntries = config.numProducers * config.entriesPerProducer;
    size_t estimatedDataSize = 0;
    for (const auto& [entry, filename] : entries) {
        estimatedDataSize += entry.serializeGDPR().size();
    }
    double estimatedDataSizeGiB = static_cast<double>(estimatedDataSize) / (1024 * 1024 * 1024);

    std::cout << "Using pre-generated entries: " << entries.size() << " entries" << std::endl;
    std::cout << "Estimated data size: " << estimatedDataSizeGiB << " GiB" << std::endl;

    // Start logging system
    LoggingManager loggingManager(loggingConfig);
    loggingManager.startGDPR();

    auto startTime = std::chrono::high_resolution_clock::now();

    // Launch producer threads - each handles a portion of entries
    std::vector<std::future<LatencyCollector>> futures;
    int entriesPerProducer = totalEntries / config.numProducers;

    for (int i = 0; i < config.numProducers; ++i) {
        int startIndex = i * entriesPerProducer;
        int numEntries = (i == config.numProducers - 1) ? 
                        (totalEntries - startIndex) : entriesPerProducer;
        
        futures.push_back(std::async(
            std::launch::async,
            appendGDPREntriesForCompression,
            std::ref(loggingManager),
            std::ref(entries),
            startIndex,
            numEntries));
    }
    
    // Collect results from all producers
    LatencyCollector masterCollector;
    for (auto& future : futures) {
        LatencyCollector threadCollector = future.get();
        masterCollector.merge(threadCollector);
    }
    
    loggingManager.stop();
    auto endTime = std::chrono::high_resolution_clock::now();
    
    // Calculate metrics
    std::chrono::duration<double> elapsed = endTime - startTime;
    double elapsedSeconds = elapsed.count();
    
    size_t finalStorageSize = calculateDirectorySize(loggingConfig.basePath);
    double finalStorageSizeGiB = static_cast<double>(finalStorageSize) / (1024 * 1024 * 1024);
    
    // Calculate compression metrics
    double compressionRatio = static_cast<double>(estimatedDataSize) / finalStorageSize;
    double compressionReduction = (1.0 - static_cast<double>(finalStorageSize) / estimatedDataSize) * 100.0;
    
    double entriesThroughput = totalEntries / elapsedSeconds;
    double logicalThroughputGiB = estimatedDataSizeGiB / elapsedSeconds;
    double physicalThroughputGiB = finalStorageSizeGiB / elapsedSeconds;
    double avgEntrySize = static_cast<double>(estimatedDataSize) / totalEntries;
    
    // Create result
    CompressionResult result;
    result.config = config;
    result.executionTimeSeconds = elapsedSeconds;
    result.totalEntries = totalEntries;
    result.avgEntrySize = avgEntrySize;
    result.totalDataSizeGiB = estimatedDataSizeGiB;
    result.finalStorageSizeGiB = finalStorageSizeGiB;
    result.compressionRatio = compressionRatio;
    result.compressionReduction = compressionReduction;
    result.entriesThroughput = entriesThroughput;
    result.logicalThroughputGiB = logicalThroughputGiB;
    result.physicalThroughputGiB = physicalThroughputGiB;
    
    std::cout << "Completed: " << std::fixed << std::setprecision(2)
              << entriesThroughput << " entries/sec, "
              << "compression ratio: " << compressionRatio << ":1 ("
              << compressionReduction << "% reduction), "
              << "original: " << estimatedDataSizeGiB << " GiB -> "
              << "compressed: " << finalStorageSizeGiB << " GiB" << std::endl;
    
    return result;
}

// Export compression results to CSV
void exportCompressionResultsToCSV(const std::vector<CompressionResult>& results, const std::string& csvPath) {
    std::ofstream csvFile(csvPath);
    if (!csvFile.is_open()) {
        throw std::runtime_error("Failed to open CSV file: " + csvPath);
    }
    
    // Write CSV header
    csvFile << "entry_size_bytes,use_encryption,compression_level,num_producers,entries_per_producer,"
            << "zipfian_theta,num_keys,max_files,"
            << "execution_time_sec,total_entries,avg_entry_size_bytes,"
            << "total_data_gib,final_storage_gib,compression_ratio,compression_reduction_percent,"
            << "entries_per_sec,logical_throughput_gib_sec,physical_throughput_gib_sec\n";
    
    // Write data rows
    for (const auto& result : results) {
        csvFile << result.config.entrySize << ","
                << (result.config.useEncryption ? 1 : 0) << ","
                << result.config.compressionLevel << ","
                << result.config.numProducers << ","
                << result.config.entriesPerProducer << ","
                << result.config.zipfianTheta << ","
                << result.config.numKeys << ","
                << LogFileHasher::get_max_files() << ","
                << result.executionTimeSeconds << ","
                << result.totalEntries << ","
                << result.avgEntrySize << ","
                << result.totalDataSizeGiB << ","
                << result.finalStorageSizeGiB << ","
                << result.compressionRatio << ","
                << result.compressionReduction << ","
                << result.entriesThroughput << ","
                << result.logicalThroughputGiB << ","
                << result.physicalThroughputGiB << "\n";
    }
    
    csvFile.close();
    std::cout << "Compression results exported to: " << csvPath << std::endl;
    std::cout << "Total compression benchmark runs: " << results.size() << std::endl;
}

int main() {
    try {
        std::cout << "GDPR Logger Compression Rate Benchmark" << std::endl;
        std::cout << "=======================================" << std::endl;
        
        // Fixed benchmark parameters
        std::vector<int> entrySizes = {1024, 4096};
        std::vector<bool> encryptionSettings = {false, true};
        std::vector<int> compressionLevels = {0, 3, 6, 9};
        
        // Fixed configuration parameters
        const double targetDataSizeGB = 10.0;
        const int64_t targetDataSizeBytes = static_cast<int64_t>(targetDataSizeGB * 1024 * 1024 * 1024);
        const double zipfianTheta = 0.99;
        const int numKeys = 100000;
        const int numProducers = 16;
        
        std::cout << "Configuration:" << std::endl;
        std::cout << "- Target data size per benchmark: " << targetDataSizeGB << " GB" << std::endl;
        std::cout << "- Entry sizes: ";
        for (size_t i = 0; i < entrySizes.size(); ++i) {
            std::cout << entrySizes[i];
            if (i < entrySizes.size() - 1) std::cout << ", ";
        }
        std::cout << " bytes" << std::endl;
        std::cout << "- Encryption settings: OFF, ON" << std::endl;
        std::cout << "- Compression levels: ";
        for (size_t i = 0; i < compressionLevels.size(); ++i) {
            std::cout << compressionLevels[i];
            if (i < compressionLevels.size() - 1) std::cout << ", ";
        }
        std::cout << std::endl;
        std::cout << "- Fixed parameters:" << std::endl;
        std::cout << "  * Writer threads: 4" << std::endl;
        std::cout << "  * Batch size: 8192" << std::endl;
        std::cout << "  * Max segment size: 100 MB" << std::endl;
        std::cout << "  * Producer threads: " << numProducers << std::endl;
        std::cout << "  * Zipfian theta: " << zipfianTheta << std::endl;
        std::cout << "  * Unique keys: " << numKeys << std::endl;
        
        // Show entry size breakdown
        std::cout << "\nEntry size breakdown:" << std::endl;
        for (int entrySize : entrySizes) {
            int64_t baseEntries = targetDataSizeBytes / entrySize;
            double actualGB = static_cast<double>(baseEntries * entrySize) / (1024.0 * 1024.0 * 1024.0);
            std::cout << "- " << entrySize << " byte entries: " << baseEntries << " entries (~" 
                      << std::fixed << std::setprecision(2) << actualGB << " GB)" << std::endl;
        }
        
        int totalConfigurations = entrySizes.size() * encryptionSettings.size() * compressionLevels.size();
        std::cout << "\nTotal configurations to test: " << totalConfigurations << std::endl;
        
        std::vector<CompressionResult> results;
        results.reserve(totalConfigurations);
        
        int currentConfig = 0;
        
        // Generate entries for each entry size
        for (int entrySize : entrySizes) {
            // Calculate entries needed for this size
            int64_t totalEntries = targetDataSizeBytes / entrySize;
            int64_t entriesPerProducer = totalEntries / numProducers;
            totalEntries = entriesPerProducer * numProducers;
            
            // Generate entries once for this entry size
            std::cout << "\n====== Generating entries for " << entrySize << " byte entries ======" << std::endl;
            CompressionConfig tempConfig;
            tempConfig.entrySize = entrySize;
            tempConfig.numProducers = numProducers;
            tempConfig.entriesPerProducer = static_cast<int>(entriesPerProducer);
            tempConfig.zipfianTheta = zipfianTheta;
            tempConfig.numKeys = numKeys;
            
            auto entries = generateGDPREntriesForCompression(tempConfig);
            
            // Now run all experiments for this entry size
            for (bool useEncryption : encryptionSettings) {
                for (int compressionLevel : compressionLevels) {
                    currentConfig++;
                    
                    double actualDataSizeGB = static_cast<double>(totalEntries * entrySize) / (1024.0 * 1024.0 * 1024.0);
                    
                    CompressionConfig config;
                    config.entrySize = entrySize;
                    config.useEncryption = useEncryption;
                    config.compressionLevel = compressionLevel;
                    config.numProducers = numProducers;
                    config.entriesPerProducer = static_cast<int>(entriesPerProducer);
                    config.zipfianTheta = zipfianTheta;
                    config.numKeys = numKeys;
                    
                    std::cout << "\nProgress: " << currentConfig << "/" << totalConfigurations << std::endl;
                    std::cout << "Configuration: " << totalEntries << " entries (" << std::fixed << std::setprecision(2) 
                              << actualDataSizeGB << " GB) with " << entrySize << " byte entries" << std::endl;
                    
                    try {
                        CompressionResult result = runCompressionBenchmark(config, entries);
                        results.push_back(result);
                    } catch (const std::exception& e) {
                        std::cerr << "Compression benchmark failed: " << e.what() << std::endl;
                    }
                }
            }
        }
        
        std::cout << "\nAll compression benchmarks completed!" << std::endl;
        
        // Export to CSV
        std::string csvPath = "gdpruler_compression_rate_results.csv";
        exportCompressionResultsToCSV(results, csvPath);
        
        std::cout << "\nCompression benchmark completed successfully!" << std::endl;
        std::cout << "Results saved to: " << csvPath << std::endl;
        
        // Print compression analysis
        if (!results.empty()) {
            std::cout << "\n=== Compression Analysis ===" << std::endl;
            
            // Find best compression ratio
            auto bestCompressionRatio = *std::max_element(results.begin(), results.end(),
                [](const CompressionResult& a, const CompressionResult& b) {
                    return a.compressionRatio < b.compressionRatio;
                });
            
            std::cout << "Best compression ratio: " << std::fixed << std::setprecision(2) 
                      << bestCompressionRatio.compressionRatio << ":1 ("
                      << bestCompressionRatio.compressionReduction << "% reduction)" << std::endl;
            std::cout << "  Configuration: " 
                      << bestCompressionRatio.config.entrySize << " byte entries, "
                      << "encryption=" << (bestCompressionRatio.config.useEncryption ? "ON" : "OFF") << ", "
                      << "compression=" << bestCompressionRatio.config.compressionLevel << std::endl;
            
            // Find best throughput with compression
            auto bestThroughput = *std::max_element(results.begin(), results.end(),
                [](const CompressionResult& a, const CompressionResult& b) {
                    return a.entriesThroughput < b.entriesThroughput;
                });
            
            std::cout << "Best throughput: " << std::fixed << std::setprecision(0) 
                      << bestThroughput.entriesThroughput << " entries/sec" << std::endl;
            std::cout << "  Configuration: " 
                      << bestThroughput.config.entrySize << " byte entries, "
                      << "encryption=" << (bestThroughput.config.useEncryption ? "ON" : "OFF") << ", "
                      << "compression=" << bestThroughput.config.compressionLevel << std::endl;
            
            // Show compression vs no compression comparison
            std::cout << "\n=== Compression Level Comparison ===" << std::endl;
            for (int entrySize : entrySizes) {
                std::cout << "\n" << entrySize << " byte entries:" << std::endl;
                for (bool encryption : {false, true}) {
                    std::cout << "  " << (encryption ? "With" : "Without") << " encryption:" << std::endl;
                    
                    for (int compLevel : compressionLevels) {
                        auto it = std::find_if(results.begin(), results.end(),
                            [entrySize, encryption, compLevel](const CompressionResult& r) {
                                return r.config.entrySize == entrySize &&
                                       r.config.useEncryption == encryption &&
                                       r.config.compressionLevel == compLevel;
                            });
                        
                        if (it != results.end()) {
                            std::cout << "    Level " << compLevel << ": " 
                                      << std::fixed << std::setprecision(2)
                                      << it->compressionRatio << ":1 ratio ("
                                      << it->compressionReduction << "% reduction)" << std::endl;
                        }
                    }
                }
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Compression benchmark failed with error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
