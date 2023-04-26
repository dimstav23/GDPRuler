#pragma once

#include <iostream>
#include <fstream>
#include <mutex>
#include <unordered_map>
#include <filesystem>

#include "../query.hpp"

namespace controller {

/* Delimiter for the logged values */
const char log_delimiter = ',';

/**
 * Query operations enum.
*/
enum operation : uint8_t {
  invalid = 0U,
  get = 1U,
  put = 2U,
  del = 3U,
  getm = 4U,
  putm = 5U,
  delm = 6U,
  get_logs = 7U
};

/**
 * Converts operation string to respective enum.
*/
inline auto convert_operation_to_enum(const std::string& oper) -> operation {
  if (oper == "get") {
    return operation::get;
  }
  if (oper == "put") {
    return operation::put;
  }
  if (oper == "del") {
    return operation::del;
  }
  if (oper == "getm") {
    return operation::getm;
  }
  if (oper == "putm") {
    return operation::putm;
  }
  if (oper == "delm") {
    return operation::delm;
  }
  if (oper == "getLogs") {
    return operation::get_logs;
  }
  // Invalid case
  return operation::invalid;
}

/**
 * Singleton logger class to store the history of each pair in a different file.
*/
class logger {
public:
  static auto get_instance() -> logger* {
    static logger history_logger;
    return &history_logger;
  }

  /**
   * Logs the query attempt
  */
  void log_attempt(const query& query_args, const default_policy& def_policy) {
    
    // lock the mutex corresponding to the key
    std::lock_guard<std::mutex> lock(m_keys_to_mutexes[query_args.key()]);

    auto log_file = get_or_open_log_stream(query_args.key());

    // write the log
    // format is the following:
    // timestamp,user_key,operation
    *log_file << std::chrono::system_clock::now().time_since_epoch().count() << ","
              << (query_args.user_key().has_value() ? query_args.user_key().value() : def_policy.user_key()) << ","
              << convert_operation_to_enum(query_args.cmd()) << "," << std::endl;
  }

  void log_result(const query& query_args, const default_policy& def_policy, const bool& result, const std::string& new_val = {}) {
    
    // lock the mutex corresponding to the key
    std::lock_guard<std::mutex> lock(m_keys_to_mutexes[query_args.key()]);

    auto log_file = get_or_open_log_stream(query_args.key());
  
    // write the log
    // format is the following:
    // timestamp,user_key,operation,operation_result,new_value(if applicable)
    *log_file << std::chrono::system_clock::now().time_since_epoch().count() << ","
              << (query_args.user_key().has_value() ? query_args.user_key().value() : def_policy.user_key()) << ","
              << convert_operation_to_enum(query_args.cmd()) << ","
              << result << ","
              << (new_val.empty() ? "" : new_val) << std::endl;
  }

  /**
   * Logs the query result
  */
  void log_encoded_result(const query& query_args, const default_policy& def_policy, const bool& result, const std::string& new_val = {}) {
    
    // lock the mutex corresponding to the key
    std::lock_guard<std::mutex> lock(m_keys_to_mutexes[query_args.key()]);

    // Open or retrieve the file the file
    auto log_file = get_or_open_log_stream(query_args.key());

    // Encode the timestamp as a fixed-width integer type
    const int64_t timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    // Encode the user key as a string
    const std::string& user_key = query_args.user_key().value_or(def_policy.user_key());
    // Encode the operation type (3bits) and the operation result (1 bit) as a single byte
    const uint8_t operation = static_cast<uint8_t>((convert_operation_to_enum(query_args.cmd()) & 0x07U) << 1U);
    const uint8_t result_bit = (result ? 0x01U : 0x00U);
    const uint8_t operation_result = operation | result_bit;
    // Calculate the total size of the entry
    // We need the size of the data + 3 delimiters + a new line char
    size_t total_size = sizeof(timestamp) + user_key.length() + 
                                sizeof(operation_result) + new_val.length() + 
                                3 * sizeof(log_delimiter) + 1;

    // Allocate an array buffer to hold the encoded entry
    std::vector<char> buffer(total_size);
    size_t offset = 0;

    // Check if buffer is null
    if (buffer.data() == nullptr) {
      return;
    }
    
    // Encode the first entry
    memcpy(&buffer[offset], &timestamp, sizeof(timestamp));
    offset += sizeof(timestamp);
    // Encode the delimiter
    buffer[offset] = log_delimiter;
    offset += sizeof(log_delimiter);
    // Encode the second entry
    memcpy(&buffer[offset], user_key.c_str(), user_key.length());
    offset += user_key.length();
    // Encode the delimiter
    buffer[offset] = log_delimiter;
    offset += sizeof(log_delimiter);
    // Encode the third and fourth entries as a single byte
    buffer[offset] = static_cast<char>(operation_result);
    offset += sizeof(operation_result);
    // Encode the delimiter
    buffer[offset] = log_delimiter;
    offset += sizeof(log_delimiter);
    // Encode the new value string, if it's not empty
    if (!new_val.empty()) {
      memcpy(&buffer[offset], new_val.c_str(), new_val.length());
      offset += new_val.length();
    }
    // Encode the newline character
    buffer[offset] = '\n';

    // Write the encoded entry to the log file
    log_file->write(buffer.data(), static_cast<std::streamsize>(total_size));
  }


private:
  logger() {
    if (!std::filesystem::exists(logs_dir)) {
      std::filesystem::create_directory(logs_dir);
    }
  }

  auto log_file_path(const std::string& key) -> std::string {
    return logs_dir + key + log_file_extension;
  }

  auto get_or_open_log_stream(const std::string& key) -> std::shared_ptr<std::ofstream> {
    // open the key's log file output stream in append only mode if it is not already opened.
    //  store it in the m_keys_to_log_files map for fast future retrieval.
    if (!m_keys_to_log_files.contains(key) || !m_keys_to_log_files[key]->is_open()) {
      m_keys_to_log_files[key] = std::make_shared<std::ofstream>(log_file_path(key), std::ios::app);
    }
    return m_keys_to_log_files[key];
  }

  const std::string logs_dir = "logs/";

  const std::string log_file_extension = ".log";

  std::unordered_map<std::string, std::mutex> m_keys_to_mutexes;

  std::unordered_map<std::string, std::shared_ptr<std::ofstream>> m_keys_to_log_files;
};

} // namespace controller
