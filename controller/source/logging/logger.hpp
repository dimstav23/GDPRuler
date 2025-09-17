#pragma once

#include <iostream>
#include <fstream>
#include <mutex>
#include <unordered_map>
#include <list>
#include <vector>
#include <cassert>
#include <cmath>

#include "log_common.hpp"
#include "gdpr_regulator.hpp"
#include "../gdpr_filter.hpp"
#include "../query.hpp"

#define DEFAULT_LOGGER_COMPRESSION_LEVEL 6

namespace controller {

/**
 * Singleton logger class to store the history of each pair in a different file.
*/
class logger {
public:
  static auto get_instance() -> logger* {
    static logger gdpr_logger;
    return &gdpr_logger;
  }

  auto init_gdpr_logger(const std::optional<std::string>& log_path = std::nullopt) -> void {
    if (m_initialized) {
      return;
    }
    
    LoggingConfig config;
    config.basePath = log_path.value_or(m_logs_dir);
    config.baseFilename = "gdpr";
    config.maxSegmentSize = 100 * 1024 * 1024; // 100 MB per file
    config.numWriterThreads = 4;
    config.batchSize = 8192;
    config.queueCapacity = 2 * config.numWriterThreads * config.batchSize;
    config.maxExplicitProducers = 32;
    // Set the max open log files to "fd_load_factor" of the file descriptors
    config.maxOpenFiles = static_cast<size_t>(std::ceil(get_max_fds() * fd_load_factor));
    config.appendTimeout = std::chrono::seconds(5);
    #ifdef ENCRYPTION_ENABLED
    config.useEncryption = true;
    #else
    config.useEncryption = false;
    #endif
    #ifdef LOGGER_COMPRESSION_LEVEL
    config.compressionLevel = LOGGER_COMPRESSION_LEVEL;
    #else
    config.compressionLevel = DEFAULT_LOGGER_COMPRESSION_LEVEL;
    #endif

    m_logs_dir = config.basePath;
    
    m_logging_manager = std::make_unique<LoggingManager>(config);
    m_logging_manager->startGDPR();
    
    // Create LogExporter
    m_log_exporter = std::make_shared<LogExporter>(
      m_logging_manager->getStorage(), 
      config.useEncryption, 
      config.compressionLevel
    );

    // Set the LogFileHasher max files
    LogFileHasher::set_max_files(config.maxOpenFiles);

    m_initialized = true;
  }

  auto get_log_exporter() -> std::shared_ptr<LogExporter> {
    if (!m_initialized) {
      init_gdpr_logger();
    }
    return m_log_exporter;
  }

  void pauseWorkersAndFlushLogs() {
    if (m_logging_manager) {
      m_logging_manager->pauseWorkersDrainAndResume();
    }
  }
  
  // Method to get thread-local producer token
  auto get_thread_producer_token() -> BufferQueue::ProducerToken& {
    // Initialize thread-local token if needed
    if (!thread_producer_token.has_value()) {
      if (!m_initialized) {
        init_gdpr_logger();
      }
      thread_producer_token = m_logging_manager->createProducerToken();
    }
    return thread_producer_token.value();
  }

  /*
   * Logs the raw query
   * UNUSED: preserved for performance testing
   */
  void log_raw_query(const query& query_args, const default_policy& def_policy, const bool& result, std::string_view new_val = {}) {
    log_encoded_query(query_args, def_policy, result, new_val);
  }

  /*
   * Logs the encoded query
   */
  void log_encoded_query(const query& query_args, const default_policy& def_policy,
                          const bool& valid, std::string_view new_val = {}) 
  {
    if (!m_initialized) [[unlikely]] {
      std::cerr << "GDPR logger is not initialized" << std::endl;
    }

    LogEntry entry = create_gdpr_log_entry(query_args, def_policy, valid, new_val);
    
    // Use thread-local producer token
    auto& token = get_thread_producer_token();
    static constexpr size_t MAX_LOG_FILES = 1000;
    const std::string& filename = LogFileHasher::hash_key_to_filename(query_args.key());
    if (!m_logging_manager->append(std::move(entry), token, filename)) {
      std::cerr << "Failed to log GDPR entry" << std::endl;
    }
  }

  auto get_logs_dir() -> std::string_view {
    return std::string_view(this->m_logs_dir);
  }

  ~logger() {
    if (m_logging_manager) {
      m_logging_manager->stop();
    }
  }

private:
  logger() = default;
  
  std::unique_ptr<LoggingManager> m_logging_manager;
  std::shared_ptr<LogExporter> m_log_exporter;
  // Thread-local producer token
  thread_local static std::optional<BufferQueue::ProducerToken> thread_producer_token;
  bool m_initialized = false;
  std::string m_logs_dir = "./gdpr_logs";

  std::unordered_map<std::string, std::atomic<int32_t>> trusted_counters;
  std::mutex counters_mutex; // Protects the counters map structure

  int32_t get_next_counter(const std::string& key) {
    // Check if key exists (most common case)
    auto it = trusted_counters.find(key);
    if (it != trusted_counters.end()) {
      // Key exists - atomic increment
      return it->second.fetch_add(1, std::memory_order_relaxed) + 1;
    }
    
    // Key doesn't exist
    {
      std::lock_guard<std::mutex> lock(counters_mutex);
      // Double-check after acquiring lock
      auto [inserted_it, inserted] = trusted_counters.try_emplace(key, 0);
      return inserted_it->second.fetch_add(1, std::memory_order_relaxed) + 1;
    }
  }

  LogEntry create_gdpr_log_entry(const query& query_args, const default_policy& def_policy,
                                  const bool& valid, std::string_view new_val) 
  {
    /*
     Log entry format:
     - int64_t timestamp
     - Length-prefixed arbitrary query key
     - user bitset
     - uint8_t operation & validity
     - Length-prefixed arbitrary new_value (if applicable)
     */
    // Get current timestamp
    const uint64_t timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    // Get the query key as string
    std::string key = std::string(query_args.key());
    // Get user key as bitset
    std::bitset<num_users> user_key = query_args.user_key().value_or(def_policy.user_key());
    // Encode operation (3 bits) + validity (1 bit)
    auto op = convert_operation_to_enum(query_args.cmd());
    uint8_t operation_result = (static_cast<uint8_t>(op) & operation_mask) << 1U;
    operation_result |= (valid ? 0x01 : 0x00);
    // Prepare new value as payload
    std::vector<uint8_t> payload;
    if (!new_val.empty()) {
        payload.assign(new_val.begin(), new_val.end());
    }
    return LogEntry(timestamp, std::move(key), user_key, operation_result, std::move(payload));
  }
};

// Define the thread_local variable
thread_local std::optional<BufferQueue::ProducerToken> logger::thread_producer_token;

} // namespace controller
