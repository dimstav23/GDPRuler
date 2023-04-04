#pragma once
#include <iostream>
#include <fstream>
#include <mutex>
#include <unordered_map>
#include <filesystem>

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

  void log(const query& query_args, const bool& result, const std::string& message = {}) {
    
    // lock the mutex corresponding to the key
    std::lock_guard<std::mutex> lock(m_keys_to_mutexes[query_args.key()]);

    // open the key's log file output stream in append only mode
    std::ofstream log_file(log_file_path(query_args.key()), std::ios::app);

    // write the log
    log_file << std::chrono::system_clock::now().time_since_epoch().count()
             << " {key: " << query_args.key()
             << ", client:" << query_args.user_key()
             << ", operation: " << query_args.cmd() 
             << ", result: " << std::boolalpha << result;
             
    if (!message.empty()) {
      log_file << ", message: " << message;
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
