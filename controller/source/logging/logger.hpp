#pragma once

#include <iostream>
#include <fstream>
#include <mutex>
#include <unordered_map>
#include <filesystem>
#include <cstring>
#include <vector>

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
    // We need the size of the data + 3 delimiters
    size_t entry_size = sizeof(timestamp) + user_key.length() + 
                        sizeof(operation_result) + new_val.length() + 
                        3 * sizeof(log_delimiter);

    // Allocate an array buffer to hold the encoded entry and its length
    size_t buffer_size = sizeof(entry_size) + entry_size;
    std::vector<char> buffer(buffer_size);

    // Check if buffer is null
    if (buffer.data() == nullptr) [[unlikely]] {
      return;
    }
    
    size_t offset = 0;
    // Encode the total size of the entry
    memcpy(&buffer[offset], &entry_size, sizeof(entry_size));
    offset += sizeof(entry_size);
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
    }

    // Write the encoded entry to the log file
    log_file->write(buffer.data(), static_cast<std::streamsize>(buffer_size));
  }

  auto log_decode(const std::string &log_name, const int64_t timestamp_thres) 
    -> std::vector<std::string> 
  {
    std::vector<std::string> entries;
    std::filesystem::path log_path(log_name);

    if (std::filesystem::exists(log_path) && std::filesystem::is_regular_file(log_path)) {
      std::string key = extract_key_from_filename(log_name);

      // lock the mutex corresponding to the key
      std::lock_guard<std::mutex> lock(m_keys_to_mutexes[key]);
      // Open or retrieve the file the file
      auto log_file = get_or_open_log_stream(key);
      
      if (log_file->is_open()) {
        // Flush the log file buffers to ensure data is written to the file
        log_file->flush();

        // Set get pointer to the beginning of the file
        log_file->seekg(0, std::ios::beg); 

        entries = read_and_decode_log_entries(log_file, timestamp_thres);

        if (entries.empty()) {
          std::cerr << "Note: " << log_name << " is empty." << std::endl;
        }
        
        // Clear the EOF flag
        log_file->clear(); 
        // Set put pointer to the end of the file for upcoming writes
        log_file->seekp(0, std::ios::end); 
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

  auto get_logs_dir() -> std::string {
    return this->m_logs_dir;
  }

  auto get_logs_extension() -> std::string {
    return this->log_file_extension;
  } 


private:
  logger() = default;

  std::string m_logs_dir = "./logs";

  const std::string log_file_extension = ".log";

  std::unordered_map<std::string, std::mutex> m_keys_to_mutexes;

  std::unordered_map<std::string, std::shared_ptr<std::fstream>> m_keys_to_log_files;

    auto log_file_path(const std::string& key) -> std::string {
    return m_logs_dir + '/' + key + log_file_extension;
  }

  auto get_or_open_log_stream(const std::string& key) -> std::shared_ptr<std::fstream> {
    // open the key's log file output stream in append only mode if it is not already opened.
    //  store it in the m_keys_to_log_files map for fast future retrieval.
    if (!m_keys_to_log_files.contains(key) || !m_keys_to_log_files[key]->is_open()) {
      m_keys_to_log_files[key] = std::make_shared<std::fstream>
                                (log_file_path(key), std::ios::in | std::ios::out | std::ios::app);
    }
    return m_keys_to_log_files[key];
  }

  auto extract_key_from_filename(const std::string& filename) -> std::string {
    // Find the last occurrence of '/' to get the start position of the key
    size_t start_pos = filename.find_last_of('/');
    if (start_pos == std::string::npos) [[unlikely]] {
      start_pos = 0; // If '/' is not found, start from the beginning of the filename
    } else {
      start_pos += 1; // Move the start position after '/'
    }

    // Find the next occurrence of log_file_extension to get the end position of the key
    size_t end_pos = filename.find(log_file_extension, start_pos);

    // Extract the key from the substring
    if (end_pos != std::string::npos) {
      return filename.substr(start_pos, end_pos - start_pos);
    }

    // If '.' is not found, return the remaining substring
    return filename.substr(start_pos);
  }

  // Read and decode log entries from the log file
  static auto read_and_decode_log_entries(const std::shared_ptr<std::fstream>& log_file, const int64_t timestamp_thres)
    -> std::vector<std::string>
  {
    std::vector<std::string> entries;
    size_t entry_size = 0;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    while (log_file->read(reinterpret_cast<char*>(&entry_size), sizeof(entry_size))) {
      std::vector<char> entry = read_log_entry(log_file, entry_size);
      if (entry.size() == entry_size) {
        entries.push_back(decode_log_entry(std::string(entry.begin(), entry.end()), timestamp_thres));
      } else {
        std::cerr << "Error while reading log entry." << std::endl;
        break; // Error occurred while reading entry
      }
    }
    return entries;
  }

  // Read a single entry from the log file
  static auto read_log_entry(const std::shared_ptr<std::fstream>& log_file, size_t entry_size)
    -> std::vector<char>
  {
    std::vector<char> entry(entry_size);
    size_t bytes_read = 0;
    while (bytes_read < entry_size) {
      if (!log_file->read(&entry[bytes_read], static_cast<std::streamsize>(entry_size - bytes_read))) {
        break; // Error occurred while reading entry
      }
      bytes_read += static_cast<size_t>(log_file->gcount());
    }
    return entry;
  }

  // Decode the given entry if its timestamp is less (earlier) than the threshold
  static auto decode_log_entry(const std::string &entry, const int64_t timestamp_thres)
    -> std::string 
  {
    // initialize the variables with default values
    int64_t timestamp = 0;
    std::string user_key;
    uint8_t operation_result = 0;
    std::string new_value;
    
    // extract the timestamp field as an int64_t
    std::memcpy(&timestamp, entry.c_str(), sizeof(timestamp));
    // only decode entries with timestamp before the getLogs() query
    if (timestamp > timestamp_thres) {
      return "";
    }
    
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
    uint8_t result_bit = operation_result & 0x01U;
    std::string valid = (result_bit != 0U) ? "valid" : "invalid";
    uint8_t operation_bits = static_cast<uint8_t>(operation_result >> 1U) & operation_mask;
    std::string oper = convert_enum_to_operation(static_cast<operation>(operation_bits));
    
    // move the start position past the delimiter
    start_pos = end_pos + 1;

    // extract the new value field (if present) as a std::string
    if (start_pos != entry.end()) {
        new_value = std::string(start_pos, entry.end());
    }

    // create a stringstream to format the output string
    std::stringstream formatted_entry;
    formatted_entry << "Timestamp: " << timestamp_to_datetime(timestamp) << ", "
                    << "User key: " << user_key << ", "
                    << "Operation: " << oper << ", "
                    << "Result: " << valid;

    if (!new_value.empty()) {
      formatted_entry << ", " << gdpr_metadata_fmt(new_value);
    }

    return formatted_entry.str();
  }
};

} // namespace controller
