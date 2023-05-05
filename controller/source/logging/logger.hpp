#pragma once

#include <iostream>
#include <fstream>
#include <mutex>
#include <unordered_map>
#include <filesystem>
#include <cstring>
#include <vector>

#include "../gdpr_filter.hpp"
#include "../query.hpp"

namespace controller {

/* Delimiter for the logged values */
const char log_delimiter = ',';
const unsigned int operation_mask = 0x07U;

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
 * Converts the enum to its respective operation string.
*/
inline auto convert_enum_to_operation(const operation oper) -> std::string  {
  if (oper == operation::get) {
    return "get";
  }
  if (oper == operation::put) {
    return "put";
  }
  if (oper == operation::del) {
    return "del";
  }
  if (oper == operation::getm) {
    return "getm";
  }
  if (oper == operation::putm) {
    return "putm";
  }
  if (oper == operation::delm) {
    return "delm";
  }
  if (oper == operation::get_logs) {
    return "getLogs";
  }
  // Invalid case
  return "invalid_op";
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

  /*
   * Set the directory where the log files reside 
   * If no value is provided, use the default path
   */
  auto init_log_path(const std::optional<std::string>& log_path = std::nullopt) -> void {
    if (log_path.has_value()) {
      m_logs_dir = log_path.value();
    }
    if (!std::filesystem::exists(m_logs_dir)) {
      std::filesystem::create_directory(m_logs_dir);
    }
  }

  /*
   * Logs the raw query -- preserved for performance testing
   */
  void log_raw_query(const query& query_args, const default_policy& def_policy, const bool& result, const std::string& new_val = {}) {
    
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

  /*
   * Logs the encoded query
   */
  void log_encoded_query(const query& query_args, const default_policy& def_policy, 
                         const bool& valid, const std::string& new_val = {}) 
  {  
    // lock the mutex corresponding to the key
    std::lock_guard<std::mutex> lock(m_keys_to_mutexes[query_args.key()]);

    // Open or retrieve the file the file
    auto log_file = get_or_open_log_stream(query_args.key());

    // Encode the timestamp as a fixed-width integer type
    const int64_t timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    // Encode the user key as a string
    const std::string& user_key = query_args.user_key().value_or(def_policy.user_key());
    // Encode the operation type (3bits) and the operation result (1 bit) as a single byte
    const uint8_t operation = static_cast<uint8_t>((convert_operation_to_enum(query_args.cmd()) & operation_mask) << 1U);
    const uint8_t valid_bit = (valid ? 0x01U : 0x00U);
    const uint8_t operation_result = operation | valid_bit;
    // Calculate the total size of the entry
    // We need the size of the data + 3 delimiters + a new line char
    size_t total_size = sizeof(timestamp) + user_key.length() + 
                                sizeof(operation_result) + new_val.length() + 
                                3 * sizeof(log_delimiter) + 1;

    // Allocate an array buffer to hold the encoded entry
    std::vector<char> buffer(total_size);
    size_t offset = 0;

    // Check if buffer is null
    if (buffer.data() == nullptr) [[unlikely]] {
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

  static auto log_decode(const std::string &log_name) -> std::vector<std::string> {
    std::vector<std::string> entries;
    std::filesystem::path log_path(log_name);

    if (std::filesystem::exists(log_path) && std::filesystem::is_regular_file(log_path)) {
      std::ifstream log_file(log_name);
      if (log_file.is_open()) {

        std::string line;
        while (std::getline(log_file, line)) {
          entries.push_back(log_entry_decode(line));
        }

        if (entries.empty()) {
          std::cerr << "Note: " << log_name << " is empty." << std::endl;
        }

        log_file.close();
      }
      else {
        std::cerr << "Error: Failed to open " << log_name << " for reading." << std::endl;
      }
    } 
    else {
      std::cerr << "Error: " << log_name << " does not exist or is not a regular file." << std::endl;
    }

    return entries;
  }

  static auto log_entry_decode(const std::string &entry) -> std::string {
    // initialize the variables with default values
    int64_t timestamp = 0;
    std::string user_key;
    uint8_t operation_result = 0;
    std::string new_value;
    
    // extract the timestamp field as an int64_t
    std::memcpy(&timestamp, entry.c_str(), sizeof(timestamp));
    
    // move past the first delimiter after the timestamp
    auto start_pos = entry.begin() + sizeof(timestamp) + sizeof(log_delimiter);
    // find the next delimiter
    auto end_pos = std::find(start_pos, entry.end(), log_delimiter);

    // extract the user key field as a std::string
    user_key = std::string(start_pos, end_pos);
    
    // move the start and end positions past the delimiter
    start_pos = end_pos + 1;
    end_pos = std::find(start_pos, entry.end(), log_delimiter);

    // extract the operation and result field as a uint8_t
    operation_result = static_cast<uint8_t>(*start_pos);
    std::string valid = ((operation_result & 0x01U) != 0U) ? "valid" : "invalid";
    std::string oper = convert_enum_to_operation(static_cast<operation>((operation_result >> 1U) & operation_mask));
    
    // move the start position past the delimiter
    start_pos = end_pos + 1;

    // extract the new value field (if present) as a std::string
    if (start_pos != entry.end()) {
        new_value = std::string(start_pos, entry.end());
    }

    // create a stringstream to format the output string
    std::stringstream formatted_entry;
    formatted_entry << "Timestamp: " << timestamp << ", "
                    << "User key: " << user_key << ", "
                    << "Operation: " << oper << ", "
                    << "Result: " << valid;

    if (!new_value.empty()) {
      formatted_entry << ", New value: " << new_value;
    }

    return formatted_entry.str();
  }

  auto get_logs_dir() -> std::string {
    return this->m_logs_dir;
  }

  auto get_logs_extension() -> std::string {
    return this->log_file_extension;
  } 


private:
  logger() = default;

  auto log_file_path(const std::string& key) -> std::string {
    return m_logs_dir + '/' + key + log_file_extension;
  }

  auto get_or_open_log_stream(const std::string& key) -> std::shared_ptr<std::ofstream> {
    // open the key's log file output stream in append only mode if it is not already opened.
    //  store it in the m_keys_to_log_files map for fast future retrieval.
    if (!m_keys_to_log_files.contains(key) || !m_keys_to_log_files[key]->is_open()) {
      m_keys_to_log_files[key] = std::make_shared<std::ofstream>(log_file_path(key), std::ios::app);
    }
    return m_keys_to_log_files[key];
  }

  std::string m_logs_dir = "./logs";

  const std::string log_file_extension = ".log";

  std::unordered_map<std::string, std::mutex> m_keys_to_mutexes;

  std::unordered_map<std::string, std::shared_ptr<std::ofstream>> m_keys_to_log_files;
};

} // namespace controller
