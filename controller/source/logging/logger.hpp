#pragma once

#include <iostream>
#include <fstream>
#include <mutex>
#include <unordered_map>
#include <filesystem>

#include "../query.hpp"

namespace controller {

/**
 * Query operations enum.
*/
enum operation {
  invalid = 0,
  get = 1,
  put = 2,
  del = 3,
  getm = 4,
  putm = 5,
  delm = 6,
  get_logs = 7
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
