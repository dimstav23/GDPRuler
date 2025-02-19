#pragma once

#include <iostream>
#include <fstream>
#include <mutex>
#include <unordered_map>
#include <list>
#include <filesystem>
#include <cstring>
#include <vector>
#include <cassert>
#include <cmath>
#include <string_view>

#include "log_common.hpp"
#include "../gdpr_filter.hpp"
#include "../query.hpp"

#include "../encryption/cipher_engine.hpp"

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
   * Logs the raw query
   * UNUSED: preserved for performance testing
   */
  void log_raw_query(const query& query_args, const default_policy& def_policy, const bool& result, std::string_view new_val = {}) {
    
    // lock the mutex corresponding to the key
    std::lock_guard<std::mutex> lock(m_keys_to_mutexes[std::string(query_args.key())]);

    auto log_file = get_or_open_log_stream(query_args.key());
  
    // write the log
    // format is the following:
    // timestamp,user_key,operation,operation_result,new_value(if applicable)
    *log_file << std::chrono::system_clock::now().time_since_epoch().count() << ","
              << query_args.user_key().value_or(def_policy.user_key()) << ","
              << convert_operation_to_enum(query_args.cmd()) << ","
              << result << ","
              << new_val << std::endl;
  }

  /*
   * Logs the encoded query
   */
  void log_encoded_query(const query& query_args, const default_policy& def_policy, 
                         const bool& valid, std::string_view new_val = {})
  {
    // Open or retrieve the file
    auto log_file = get_or_open_log_stream(query_args.key());

    // Lock the mutex corresponding to the key for the whole function scope
    std::lock_guard<std::mutex> lock(m_keys_to_mutexes[std::string(query_args.key())]);

    // To avoid potential race conditions
    if (!log_file->is_open()) {
      log_file = get_or_open_log_stream(query_args.key());
    }

    // Encode the timestamp as a fixed-width integer type
    const int64_t timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    // Encode the user key as a string
    std::string_view user_key = query_args.user_key().value_or(def_policy.user_key());
    // Encode the operation type (3bits) and the operation result (1 bit) as a single byte
    const uint8_t operation = static_cast<uint8_t>((convert_operation_to_enum(query_args.cmd()) & operation_mask) << 1U);
    const uint8_t valid_bit = (valid ? 0x01U : 0x00U);
    const uint8_t operation_result = operation | valid_bit;
    // Calculate the total size of the entry
    // We need the size of the data + 3 delimiters
    size_t entry_size = sizeof(timestamp) + user_key.length() + 
                        sizeof(operation_result) + new_val.length() + 
                        3 * sizeof(log_delimiter);

    // Allocate an array buffer to hold the encoded entry
    std::vector<char> buffer(entry_size);

    // Check if buffer is null
    if (buffer.data() == nullptr) [[unlikely]] {
      return;
    }
    
    size_t offset = 0;
    // Encode the timestamp entry
    memcpy(&buffer[offset], &timestamp, sizeof(timestamp));
    offset += sizeof(timestamp);
    // Encode the delimiter
    buffer[offset] = log_delimiter;
    offset += sizeof(log_delimiter);
    // Encode the second entry
    memcpy(&buffer[offset], user_key.data(), user_key.length());
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
      memcpy(&buffer[offset], new_val.data(), new_val.length());
    }

    #ifndef ENCRYPTION_ENABLED
    // Write the encoded entry size to the log file
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    log_file->write(reinterpret_cast<const char*>(&entry_size), sizeof(entry_size));
    // Write the encoded entry to the log file
    log_file->write(buffer.data(), static_cast<std::streamsize>(entry_size));
    #else
    // important: pass buffer.begin() and buffer.end() as encrypt will look for 
    // null termination character otherwise
    auto encrypt_result = m_cipher->encrypt(std::string_view(buffer.data(), buffer.size()),
                                            cipher_key_type::log_key);
    if (encrypt_result.m_success) {
      // Write the encrypted entry size to the log file
      entry_size = encrypt_result.m_ciphertext.size();
      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
      log_file->write(reinterpret_cast<const char*>(&entry_size), sizeof(entry_size));
      log_file->write(encrypt_result.m_ciphertext.data(),
                      static_cast<std::streamsize>(encrypt_result.m_ciphertext.size()));
    }
    else {
      std::cerr << "Error in writing log entry: Encryption failed" << std::endl;
    }
    #endif
  }

  auto log_decode(std::string_view log_name, const int64_t timestamp_thres)
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

  auto get_logs_dir() -> std::string_view {
    return std::string_view(this->m_logs_dir);
  }

  auto get_logs_extension() -> std::string_view {
    return this->log_file_extension;
  } 


private:
  // Set the max open log files to "fd_load_factor" of the file descriptors
  logger() : m_max_open_log_files(static_cast<size_t>(std::ceil(get_max_fds() * fd_load_factor))) {}

  std::string m_logs_dir = "./logs";

  const std::string_view log_file_extension = ".log";

  std::unordered_map<std::string, std::mutex> m_keys_to_mutexes;
  std::unordered_map<std::string, std::shared_ptr<std::fstream>> m_keys_to_log_files;
  size_t m_max_open_log_files;
  std::mutex m_fd_mgmt_mutex;
  std::list<std::string> m_used_keys;

  controller::cipher_engine* m_cipher = controller::cipher_engine::get_instance();

  auto log_file_path(std::string_view key) -> std::string {
    return m_logs_dir + '/' + std::string(key) + std::string(log_file_extension);
  }

  /*
   * Open the key's log file output stream in append only mode if it is not already opened.
   * Store it in the m_keys_to_log_files map for fast future retrieval.
   * Depending on the number of available file descriptors and if their limit (80%) is reached,
   * close the files that were opened based on chronological order. This might not be optimal,
   * but it's "cheaper" performance-wise than enforcing a sophisticated policy (e.g. LRU).
   */
  auto get_or_open_log_stream(std::string_view key) -> std::shared_ptr<std::fstream> {
    // Lock the mutex for the whole function scope to perform the metadata management & eviction
    std::lock_guard<std::mutex> lock(m_fd_mgmt_mutex);

    std::string key_str = std::string(key);

    if (!m_keys_to_mutexes.contains(key_str)) {
      // initialize the mutex associated with the key
      m_keys_to_mutexes[key_str];
    }

    if (!m_keys_to_log_files.contains(key_str) || !m_keys_to_log_files[key_str]->is_open()) {
      // If the map is at its maximum size, evict the oldest FD
      if (m_keys_to_log_files.size() >= m_max_open_log_files) {
        close_older_fd();
      }
      // Put the key in the list of the already used_keys
      m_used_keys.push_back(key_str);
      m_keys_to_log_files[key_str] = std::make_shared<std::fstream>
                                (log_file_path(key), std::ios::in | std::ios::out | std::ios::app);
    }

    return m_keys_to_log_files[key_str];
  }

  auto close_older_fd() -> void {
    // Try to remove the least recently used key from the map and list
    // If it doesn't succeed because the key is used (judging by its mutex)
    // Then try the next one
    for (auto it = m_used_keys.begin(); it != m_used_keys.end(); ++it) {
      auto key_to_remove = *it;
      // Try to lock the mutex corresponding to the key -- unique_lock for the try_to_lock option
      std::unique_lock<std::mutex> lock(m_keys_to_mutexes[key_to_remove], std::try_to_lock);
      if (lock.owns_lock()) {
        // Close the file stream associated with the key
        m_keys_to_log_files[key_to_remove]->close();

        m_used_keys.erase(it); // Remove the locked element
        m_keys_to_log_files.erase(key_to_remove);

        // Exit the loop if a lock is acquired successfully
        // The lock will be released when it goes out of scope
        break;
      }
    }
  }

  auto extract_key_from_filename(std::string_view filename) -> std::string {
    // Find the last occurrence of '/' to get the start position of the key
    size_t start_pos = filename.find_last_of('/');
    if (start_pos == std::string_view::npos) [[unlikely]] {
      start_pos = 0; // If '/' is not found, start from the beginning of the filename
    } else {
      start_pos += 1; // Move the start position after '/'
    }

    // Find the next occurrence of log_file_extension to get the end position of the key
    size_t end_pos = filename.find(log_file_extension, start_pos);

    // Extract the key from the substring
    if (end_pos != std::string_view::npos) {
      return std::string(filename.substr(start_pos, end_pos - start_pos));
    }

    // If '.' is not found, return the remaining substring
    return std::string(filename.substr(start_pos));
  }

  // Read and decode log entries from the log file
  auto read_and_decode_log_entries(const std::shared_ptr<std::fstream>& log_file, const int64_t timestamp_thres)
    -> std::vector<std::string>
  {
    std::vector<std::string> entries;
    size_t entry_size = 0;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    while (log_file->read(reinterpret_cast<char*>(&entry_size), sizeof(entry_size))) {
      std::vector<char> entry = read_log_entry(log_file, entry_size);
      if (entry.size() == entry_size) {
        #ifndef ENCRYPTION_ENABLED
        entries.push_back(decode_log_entry(std::string_view(entry.data(), entry.size()), timestamp_thres));
        #else
        std::string decrypted_entry = decrypt_log_entry(std::string_view(entry.data(), entry.size()));
        entries.push_back(decode_log_entry(decrypted_entry, timestamp_thres));
        #endif        
      } else {
        std::cerr << "Error while reading log entry." << std::endl;
        break; // Error occurred while reading entry
      }
    }
    return entries;
  }

  // Read a single entry from the log file
  auto read_log_entry(const std::shared_ptr<std::fstream>& log_file, size_t entry_size)
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
  auto decode_log_entry(std::string_view entry, const int64_t timestamp_thres)
    -> std::string 
  {
    // initialize the variables with default values
    int64_t timestamp = 0;
    // extract the timestamp field as an int64_t
    std::memcpy(&timestamp, entry.data(), sizeof(timestamp));
    // only decode entries with timestamp before the getLogs() query
    if (timestamp > timestamp_thres) {
      return "";
    }
    
    // move past the first delimiter after the timestamp
    auto start_pos = entry.begin() + sizeof(timestamp) + sizeof(log_delimiter);
    // find the next delimiter
    auto end_pos = std::find(start_pos, entry.end(), log_delimiter);

    // extract the user key field as a std::string
    std::string_view user_key(start_pos, end_pos - start_pos);
    
    // move the start and end positions past the delimiter
    start_pos = end_pos + 1;
    end_pos = std::find(start_pos, entry.end(), log_delimiter);

    // extract the operation and result field as a uint8_t
    uint8_t operation_result = static_cast<uint8_t>(*start_pos);
    uint8_t result_bit = operation_result & 0x01U;
    std::string_view valid = (result_bit != 0U) ? "valid" : "invalid";
    uint8_t operation_bits = static_cast<uint8_t>(operation_result >> 1U) & operation_mask;
    std::string_view oper = convert_enum_to_operation(static_cast<operation>(operation_bits));
    
    // move the start position past the delimiter
    start_pos = end_pos + 1;

    // extract the new value field (if present) as a std::string
    std::string_view new_value;
    if (start_pos != entry.end()) {
      new_value = std::string_view(&*start_pos, entry.end() - start_pos);
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

  auto decrypt_log_entry(std::string_view entry) -> std::string {
    // Decrypt the entry
    auto decrypt_result = m_cipher->decrypt(entry, cipher_key_type::log_key);
    if (!decrypt_result.m_success) {
      std::cerr << "Error in decrypting log entry" << std::endl;
      return "";
    }
    return decrypt_result.m_plaintext;
  }
};

} // namespace controller
