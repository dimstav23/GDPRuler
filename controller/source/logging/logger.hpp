#pragma once

#include <iostream>
#include <fstream>
#include <mutex>
#include <unordered_map>
#include <filesystem>

namespace controller {

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

  void log(const query& query_args, const bool& result, const std::string& new_val = {}) {
    
    // lock the mutex corresponding to the key
    std::lock_guard<std::mutex> lock(m_keys_to_mutexes[query_args.key()]);

    // open the key's log file output stream in append only mode
    std::ofstream log_file(log_file_path(query_args.key()), std::ios::app);

    // write the log
    log_file << std::chrono::system_clock::now().time_since_epoch().count()
             << " {client: " << (query_args.user_key().has_value() ? query_args.user_key().value() : "")
             << ", oper: " << convert_operation_to_enum(query_args.cmd())
             << ", res: " << result;
             
    if (!new_val.empty()) {
      log_file << ", newVal: " << new_val;
    }

    log_file << "}" << std::endl;
  }


private:
  logger() {
    if (!std::filesystem::exists("logs")) {
      std::filesystem::create_directory("logs");
    }
  }

  const std::string logs_dir = "logs/";

  const std::string log_file_extension = ".log";

  auto log_file_path(const std::string& key) -> std::string {
    return logs_dir + key + log_file_extension;
  }

  std::unordered_map<std::string, std::mutex> m_keys_to_mutexes;
};

} // namespace controller
