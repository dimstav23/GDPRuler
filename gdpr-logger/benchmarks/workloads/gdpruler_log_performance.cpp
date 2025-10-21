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

// Configuration for each benchmark run
struct BenchmarkConfig {
    int numConsumerThreads;    // Writer threads: 1, 4, 8
    int batchSize;            // Batch sizes: 1-8192
    int entrySize;            // Entry sizes: 64, 1024, 4096 bytes
    int numProducers;         // Number of producer threads: 1, 16, 32
    int entriesPerProducer;   // Entries per producer
    double zipfianTheta;      // Zipfian distribution parameter
    int numKeys;              // Total number of unique keys
    bool useEncryption;       // Encryption on/off
    int compressionLevel;     // Compression levels: 0, 3, 6
    int repeats;              // Number of repeat runs
};

// Results for each benchmark run
struct BenchmarkResult {
    BenchmarkConfig config;
    double executionTimeSeconds;
    size_t totalEntries;
    double avgEntrySize;
    double totalDataSizeGiB;
    double finalStorageSizeGiB;
    double writeAmplification;
    double entriesThroughput;
    double logicalThroughputGiB;
    double physicalThroughputGiB;
    double avgLatencyMs;
    double medianLatencyMs;
    double maxLatencyMs;
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

// Instead of generating batches, generate individual entries
std::vector<std::pair<LogEntry, std::string>> generateGDPREntries(const BenchmarkConfig& config) {
    std::vector<std::pair<LogEntry, std::string>> entries;
    ZipfianGenerator zipfGen(config.numKeys, config.zipfianTheta);
    
    int totalEntries = config.numProducers * config.entriesPerProducer;
    entries.reserve(totalEntries);
    
    std::cout << "Generating " << totalEntries << " GDPR entries with Zipfian distribution..." << std::flush;
    
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
        
        // Hash the key to get filename (like your actual code)
        std::string filename = LogFileHasher::hash_key_to_filename(gdprKey);
        
        entries.emplace_back(std::move(entry), std::move(filename));
    }
    
    std::cout << " Done." << std::endl;
    return entries;
}

// Run a single benchmark configuration with repeat functionality
BenchmarkResult runBenchmarkWithRepeats(const BenchmarkConfig& config, 
                                        const std::vector<std::pair<LogEntry, std::string>>& entries) {
    std::vector<BenchmarkResult> results;
    results.reserve(config.repeats);
    
    std::cout << "\n=========================================" << std::endl;
    std::cout << "Running benchmark (" << config.repeats << " repeats): " 
              << config.numConsumerThreads << " consumers, "
              << config.batchSize << " batch size, "
              << config.entrySize << " byte entries, "
              << config.numProducers << " producers, "
              << "encryption=" << (config.useEncryption ? "ON" : "OFF") << ", "
              << "compression=" << config.compressionLevel << std::endl;
    std::cout << "=========================================" << std::endl;
    
    for (int repeat = 0; repeat < config.repeats; ++repeat) {
        std::cout << "\n--- Repeat " << (repeat + 1) << "/" << config.repeats << " ---" << std::endl;
        
        // Setup logging configuration
        LoggingConfig loggingConfig;
        loggingConfig.basePath = "/scratch/dimitrios/gdpruler_fs/gdpr_benchmark_logs";
        loggingConfig.baseFilename = "gdpr";
        loggingConfig.maxSegmentSize = 100 * 1024 * 1024; // 100 MB
        loggingConfig.maxAttempts = 5;
        loggingConfig.baseRetryDelay = std::chrono::milliseconds(1);
        loggingConfig.queueCapacity = config.numProducers * config.entriesPerProducer * 2;
        loggingConfig.maxExplicitProducers = config.numProducers;
        loggingConfig.batchSize = config.batchSize;
        loggingConfig.numWriterThreads = config.numConsumerThreads;
        loggingConfig.appendTimeout = std::chrono::minutes(5);
        loggingConfig.useEncryption = config.useEncryption;
        loggingConfig.compressionLevel = config.compressionLevel;
        
        // Configure LogFileHasher and set reasonable maxOpenFiles
        struct rlimit limits{};
        if (getrlimit(RLIMIT_NOFILE, &limits) == 0) {
            LogFileHasher::set_max_files(0.9 * static_cast<int>(limits.rlim_cur));
        }
        loggingConfig.maxOpenFiles = LogFileHasher::get_max_files();
        
        // Setup benchmark directory (delete and recreate for each repeat)
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
                appendGDPREntriesIndividually,
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
        double writeAmplification = static_cast<double>(finalStorageSize) / estimatedDataSize;
        
        double entriesThroughput = totalEntries / elapsedSeconds;
        double logicalThroughputGiB = estimatedDataSizeGiB / elapsedSeconds;
        double physicalThroughputGiB = finalStorageSizeGiB / elapsedSeconds;
        double avgEntrySize = static_cast<double>(estimatedDataSize) / totalEntries;
        
        auto latencyStats = calculateLatencyStats(masterCollector);
        
        // Create result for this repeat
        BenchmarkResult result;
        result.config = config;
        result.executionTimeSeconds = elapsedSeconds;
        result.totalEntries = totalEntries;
        result.avgEntrySize = avgEntrySize;
        result.totalDataSizeGiB = estimatedDataSizeGiB;
        result.finalStorageSizeGiB = finalStorageSizeGiB;
        result.writeAmplification = writeAmplification;
        result.entriesThroughput = entriesThroughput;
        result.logicalThroughputGiB = logicalThroughputGiB;
        result.physicalThroughputGiB = physicalThroughputGiB;
        result.avgLatencyMs = latencyStats.avgMs;
        result.medianLatencyMs = latencyStats.medianMs;
        result.maxLatencyMs = latencyStats.maxMs;
        
        results.push_back(result);
        
        std::cout << "Repeat " << (repeat + 1) << " completed: " << entriesThroughput << " entries/sec, "
                  << "write amplification: " << writeAmplification << std::endl;
    }
    
    // Calculate averages across all repeats
    BenchmarkResult avgResult;
    avgResult.config = config;
    avgResult.totalEntries = results[0].totalEntries;
    avgResult.avgEntrySize = results[0].avgEntrySize;
    avgResult.totalDataSizeGiB = results[0].totalDataSizeGiB;
    
    // Average the performance metrics
    avgResult.executionTimeSeconds = 0;
    avgResult.finalStorageSizeGiB = 0;
    avgResult.writeAmplification = 0;
    avgResult.entriesThroughput = 0;
    avgResult.logicalThroughputGiB = 0;
    avgResult.physicalThroughputGiB = 0;
    avgResult.avgLatencyMs = 0;
    avgResult.medianLatencyMs = 0;
    avgResult.maxLatencyMs = 0;
    
    for (const auto& result : results) {
        avgResult.executionTimeSeconds += result.executionTimeSeconds;
        avgResult.finalStorageSizeGiB += result.finalStorageSizeGiB;
        avgResult.writeAmplification += result.writeAmplification;
        avgResult.entriesThroughput += result.entriesThroughput;
        avgResult.logicalThroughputGiB += result.logicalThroughputGiB;
        avgResult.physicalThroughputGiB += result.physicalThroughputGiB;
        avgResult.avgLatencyMs += result.avgLatencyMs;
        avgResult.medianLatencyMs += result.medianLatencyMs;
        avgResult.maxLatencyMs += result.maxLatencyMs;
    }
    
    int numResults = static_cast<int>(results.size());
    avgResult.executionTimeSeconds /= numResults;
    avgResult.finalStorageSizeGiB /= numResults;
    avgResult.writeAmplification /= numResults;
    avgResult.entriesThroughput /= numResults;
    avgResult.logicalThroughputGiB /= numResults;
    avgResult.physicalThroughputGiB /= numResults;
    avgResult.avgLatencyMs /= numResults;
    avgResult.medianLatencyMs /= numResults;
    avgResult.maxLatencyMs /= numResults;
    
    std::cout << "\n--- Average across " << config.repeats << " repeats ---" << std::endl;
    std::cout << "Average throughput: " << avgResult.entriesThroughput << " entries/sec" << std::endl;
    std::cout << "Average write amplification: " << avgResult.writeAmplification << std::endl;
    
    return avgResult;
}

// Export results to CSV
void exportResultsToCSV(const std::vector<BenchmarkResult>& results, const std::string& csvPath) {
    std::ofstream csvFile(csvPath);
    if (!csvFile.is_open()) {
        throw std::runtime_error("Failed to open CSV file: " + csvPath);
    }
    
    // Write CSV header (updated with repeats field)
    csvFile << "consumers,batch_size,entry_size_bytes,num_producers,entries_per_producer,"
            << "zipfian_theta,num_keys,use_encryption,compression_level,repeats,max_files,"
            << "execution_time_sec,total_entries,avg_entry_size_bytes,"
            << "total_data_gib,final_storage_gib,write_amplification,entries_per_sec,"
            << "logical_throughput_gib_sec,physical_throughput_gib_sec,avg_latency_ms,"
            << "median_latency_ms,max_latency_ms\n";
    
    // Write data rows
    for (const auto& result : results) {
        csvFile << result.config.numConsumerThreads << ","
                << result.config.batchSize << ","
                << result.config.entrySize << ","
                << result.config.numProducers << ","
                << result.config.entriesPerProducer << ","
                << result.config.zipfianTheta << ","
                << result.config.numKeys << ","
                << (result.config.useEncryption ? 1 : 0) << ","
                << result.config.compressionLevel << ","
                << result.config.repeats << ","
                << LogFileHasher::get_max_files() << ","
                << result.executionTimeSeconds << ","
                << result.totalEntries << ","
                << result.avgEntrySize << ","
                << result.totalDataSizeGiB << ","
                << result.finalStorageSizeGiB << ","
                << result.writeAmplification << ","
                << result.entriesThroughput << ","
                << result.logicalThroughputGiB << ","
                << result.physicalThroughputGiB << ","
                << result.avgLatencyMs << ","
                << result.medianLatencyMs << ","
                << result.maxLatencyMs << "\n";
    }
    
    csvFile.close();
    std::cout << "Results exported to: " << csvPath << std::endl;
    std::cout << "Total benchmark runs: " << results.size() << std::endl;
}

int main() {
    try {
        std::cout << "GDPR Logger Performance Benchmark" << std::endl;
        std::cout << "==================================" << std::endl;
        
        // Define benchmark parameters
        std::vector<int> consumerThreadCounts = {4, 8};
        std::vector<int> batchSizes = {512, 2048, 8192};
        std::vector<int> entrySizes = {256, 1024, 4096};
        std::vector<int> producerCounts = {16};
        std::vector<bool> encryptionSettings = {true};
        std::vector<int> compressionLevels = {0};
        
        // Number of repeats for each configuration
        const int numRepeats = 3;

        // Target 10GB of logical data per benchmark
        const double targetDataSizeGB = 10.0;
        const int64_t targetDataSizeBytes = static_cast<int64_t>(targetDataSizeGB * 1024 * 1024 * 1024);
        
        const double zipfianTheta = 0.99; // Realistic Zipfian parameter
        const int numKeys = 100000; // Total unique keys
        
        std::cout << "Configuration:" << std::endl;
        std::cout << "- Target data size per benchmark: " << targetDataSizeGB << " GB" << std::endl;
        std::cout << "- Number of repeats per configuration: " << numRepeats << std::endl;
        std::cout << "- Producer counts: ";
        for (size_t i = 0; i < producerCounts.size(); ++i) {
            std::cout << producerCounts[i];
            if (i < producerCounts.size() - 1) std::cout << ", ";
        }
        std::cout << std::endl;
        std::cout << "- Encryption settings: ON" << std::endl;
        std::cout << "- Compression levels: ";
        for (size_t i = 0; i < compressionLevels.size(); ++i) {
            std::cout << compressionLevels[i];
            if (i < compressionLevels.size() - 1) std::cout << ", ";
        }
        std::cout << std::endl;
        std::cout << "- Zipfian theta: " << zipfianTheta << std::endl;
        std::cout << "- Unique keys: " << numKeys << std::endl;
        
        // Show entry size breakdown
        std::cout << "\nEntry size breakdown:" << std::endl;
        for (int entrySize : entrySizes) {
            int64_t baseEntries = targetDataSizeBytes / entrySize;
            double actualGB = static_cast<double>(baseEntries * entrySize) / (1024.0 * 1024.0 * 1024.0);
            std::cout << "- " << entrySize << " byte entries: " << baseEntries << " entries (~" 
                      << std::fixed << std::setprecision(2) << actualGB << " GB)" << std::endl;
        }
        
        int totalConfigurations = consumerThreadCounts.size() * batchSizes.size() * entrySizes.size() 
                                  * producerCounts.size() * encryptionSettings.size() * compressionLevels.size();
        std::cout << "\nTotal configurations to test: " << totalConfigurations << std::endl;
        std::cout << "Total benchmark runs (including repeats): " << totalConfigurations * numRepeats << std::endl;
        
        std::vector<BenchmarkResult> results;
        results.reserve(totalConfigurations);
        
        int currentConfig = 0;
        for (int entrySize : entrySizes) {
            // Calculate entries needed for this size
            int64_t totalEntries = targetDataSizeBytes / entrySize;
            int64_t entriesPerProducer = totalEntries / producerCounts[0]; // Use first and only producer count
            totalEntries = entriesPerProducer * producerCounts[0];
            
            // GENERATE ENTRIES ONCE for this entry size
            std::cout << "\n====== Generating benchmark entries for " << entrySize << " byte entries ======" << std::endl;
            BenchmarkConfig tempConfig;
            tempConfig.entrySize = entrySize;
            tempConfig.numProducers = producerCounts[0];
            tempConfig.entriesPerProducer = static_cast<int>(entriesPerProducer);
            tempConfig.zipfianTheta = zipfianTheta;
            tempConfig.numKeys = numKeys;
            tempConfig.repeats = numRepeats;
            
            auto entries = generateGDPREntries(tempConfig);
            
            // Now run all experiments for this entry size
            for (int consumers : consumerThreadCounts) {
                for (int batchSize : batchSizes) {
                    for (int numProducers : producerCounts) {
                        for (bool useEncryption : encryptionSettings) {
                            for (int compressionLevel : compressionLevels) {
                                currentConfig++;
                                
                                double actualDataSizeGB = static_cast<double>(totalEntries * entrySize) / (1024.0 * 1024.0 * 1024.0);
                                
                                BenchmarkConfig config;
                                config.numConsumerThreads = consumers;
                                config.batchSize = batchSize;
                                config.entrySize = entrySize;
                                config.numProducers = numProducers;
                                config.entriesPerProducer = static_cast<int>(entriesPerProducer);
                                config.zipfianTheta = zipfianTheta;
                                config.numKeys = numKeys;
                                config.useEncryption = useEncryption;
                                config.compressionLevel = compressionLevel;
                                config.repeats = numRepeats;
                                
                                std::cout << "\nProgress: " << currentConfig << "/" << totalConfigurations << std::endl;
                                std::cout << "Configuration: " << totalEntries << " entries (" << std::fixed << std::setprecision(2) 
                                          << actualDataSizeGB << " GB) with " << entrySize << " byte entries" << std::endl;
                                
                                try {
                                    // Run benchmark with repeats and get averaged results
                                    BenchmarkResult result = runBenchmarkWithRepeats(config, entries);
                                    results.push_back(result);
                                } catch (const std::exception& e) {
                                    std::cerr << "Benchmark failed: " << e.what() << std::endl;
                                }
                            }
                        }
                    }
                }
            }
        }
        
        std::cout << "\nAll benchmarks completed!" << std::endl;
        
        // Export to CSV
        std::string csvPath = "gdpr_logger_benchmark_results.csv";
        exportResultsToCSV(results, csvPath);
        
        std::cout << "\nBenchmark completed successfully!" << std::endl;
        std::cout << "Results saved to: " << csvPath << std::endl;
        
        // Print summary statistics
        if (!results.empty()) {
            std::cout << "\n=== Summary Statistics ===" << std::endl;
            
            // Find max throughput configuration
            auto maxThroughput = *std::max_element(results.begin(), results.end(),
                [](const BenchmarkResult& a, const BenchmarkResult& b) {
                    return a.entriesThroughput < b.entriesThroughput;
                });
            
            std::cout << "Best throughput: " << maxThroughput.entriesThroughput << " entries/sec" << std::endl;
            std::cout << "  Configuration: " 
                      << maxThroughput.config.numConsumerThreads << " consumers, "
                      << maxThroughput.config.numProducers << " producers, "
                      << maxThroughput.config.batchSize << " batch size, "
                      << maxThroughput.config.entrySize << " byte entries, "
                      << "encryption=" << (maxThroughput.config.useEncryption ? "ON" : "OFF") << ", "
                      << "compression=" << maxThroughput.config.compressionLevel 
                      << " (averaged over " << maxThroughput.config.repeats << " repeats)" << std::endl;
            
            // Find best compression ratio
            auto bestCompression = *std::min_element(results.begin(), results.end(),
                [](const BenchmarkResult& a, const BenchmarkResult& b) {
                    return a.writeAmplification < b.writeAmplification;
                });
            
            std::cout << "Best compression ratio: " << bestCompression.writeAmplification << "x" << std::endl;
            std::cout << "  Configuration: " 
                      << bestCompression.config.numConsumerThreads << " consumers, "
                      << bestCompression.config.numProducers << " producers, "
                      << bestCompression.config.batchSize << " batch size, "
                      << bestCompression.config.entrySize << " byte entries, "
                      << "encryption=" << (bestCompression.config.useEncryption ? "ON" : "OFF") << ", "
                      << "compression=" << bestCompression.config.compressionLevel 
                      << " (averaged over " << bestCompression.config.repeats << " repeats)" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Benchmark failed with error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
