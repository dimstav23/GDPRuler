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
#include "../gdpr_filter.hpp"
#include "../query.hpp"

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
    config.maxSegmentSize = 10 * 1024 * 1024;
    config.useEncryption = true;
    config.compressionLevel = 9;
    config.numWriterThreads = 2;
    config.batchSize = 50;
    config.queueCapacity = 2048;
    config.maxExplicitProducers = 4;
    // Set the max open log files to "fd_load_factor" of the file descriptors
    config.maxOpenFiles = static_cast<size_t>(std::ceil(get_max_fds() * fd_load_factor));
    config.appendTimeout = std::chrono::seconds(5);

    m_logs_dir = config.basePath;
    
    m_logging_manager = std::make_unique<LoggingManager>(config);
    m_logging_manager->startGDPR();
    
    m_initialized = true;
  }

  // New method to get thread-local producer token
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
    if (!m_logging_manager->append(std::move(entry), token, std::string(query_args.key()))) {
      std::cerr << "Failed to log GDPR entry" << std::endl;
    }
  }

  auto log_decode(std::string_view log_name, const int64_t timestamp_thres) 
    -> std::vector<std::string> {
    // TODO: Implement using external logger's export functionality
    return {};
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
  // Thread-local producer token
  thread_local static std::optional<BufferQueue::ProducerToken> thread_producer_token;
  bool m_initialized = false;
  std::string m_logs_dir = "./gdpr_logs";
  int32_t trusted_counter = 0;

  LogEntry create_gdpr_log_entry(const query& query_args, const default_policy& def_policy,
                                  const bool& valid, std::string_view new_val) 
  {
    /*
     Log entry format:
     - int64_t timestamp
     - int32_t trusted_counter
     - user bitset
     - uint8_t operation & validity
     - Length-prefixed arbitrary new_value (if applicable)
     */
    // Get current timestamp
    const int64_t timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    // Get trusted counter
    int32_t cnt = ++trusted_counter;
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
    return LogEntry(timestamp, cnt, user_key, operation_result, std::move(payload));
  }
};

// Define the thread_local variable
thread_local std::optional<BufferQueue::ProducerToken> logger::thread_producer_token;

} // namespace controller
