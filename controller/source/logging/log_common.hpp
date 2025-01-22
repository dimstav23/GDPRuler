#pragma once

#include <filesystem>
#include <cstring>
#include <sys/resource.h>

#include "../common.hpp"
#include "../gdpr_metadata.hpp"

namespace controller {

/* Delimiter for the logged values */
const char log_delimiter = ',';
const unsigned int operation_mask = 0x07U;
// Set the max open log files to 80% of the file descriptors
constexpr double fd_load_factor = 0.8;

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
  get_logs = 6U
};

/**
 * Converts operation string to respective enum.
*/
inline auto convert_operation_to_enum(std::string_view oper) -> operation {
  if (oper == "get") {
    return operation::get;
  }
  if (oper == "put") {
    return operation::put;
  }
  if (oper == "delete") {
    return operation::del;
  }
  if (oper == "getm") {
    return operation::getm;
  }
  if (oper == "putm") {
    return operation::putm;
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
    return "delete";
  }
  if (oper == operation::getm) {
    return "getm";
  }
  if (oper == operation::putm) {
    return "putm";
  }
  if (oper == operation::get_logs) {
    return "getLogs";
  }
  // Invalid case
  return "invalid_op";
}

/*
 * Convert a numerical timestamp to datetime string with second precision
 */
inline auto timestamp_to_datetime(int64_t timestamp) -> std::string {
  // Convert int64_t to time_t
  const auto time = static_cast<time_t>(timestamp / s2ns);

  // Convert time_t to struct tm in a thread-safe manner
  std::tm time_info {};
  gmtime_r(&time, &time_info);

  // Format struct tm into a string
  std::ostringstream time_stream;
  time_stream << std::put_time(&time_info, "%Y-%m-%d %H:%M:%S");
  return time_stream.str();
}

/*
 * Convert encoded gdpr metadata to human readable string for regulator output
 */
inline auto gdpr_metadata_fmt(std::string_view value_str) -> std::string {
  std::string res;
  res.reserve(value_str.length());  // Reserve space to avoid reallocations

  size_t start = 0;
  size_t end = 0;
  int count = 0;

  while (count < max_gdpr_field_guard && (end = value_str.find('|', start)) != std::string_view::npos) {
    std::string_view token = value_str.substr(start, end - start);

    switch (count) {
      case usr: 
        res.append("User/Owner: ").append(token).append(", ");
        break;
      case encr:
        res.append("Encryption enabled: ").append(token == "1" ? "true" : "false").append(", ");
        break;
      case pur:
        {
          auto purposes = std::bitset<num_purposes>(std::stoull(std::string(token)));
          res.append("Purposes: ").append(get_purposes_string(purposes));
          break;
        }
      case obj:
        {
          auto objections = std::bitset<num_purposes>(std::stoull(std::string(token)));
          res.append("Objections: ").append(get_purposes_string(objections));
          break;
        }
      case org:
        res.append("Data origin: ").append(token).append(", ");
        break;
      case exp:
        {
          std::string_view expire_time = (token == "0") ?
              "none" : timestamp_to_datetime(std::stoi(std::string(token)));
          res.append("Expiration time: ").append(expire_time).append(", ");
          break;
        }
      case shr:
        res.append("Shared with: ").append(token).append(", ");
        break;
      case log:
        res.append("Log enabled: ").append(token == "1" ? "true" : "false").append(", ");
        break;
      case val:
        res.append("Value: ").append(token);
        break;
      default:
        break;
    }
    start = end + 1;
    count++;
  }

  if (count != max_gdpr_field_guard) {
    throw std::invalid_argument("Invalid GDPR metadata format in the logs");
  }

  return std::move(res);
}

/*
 * Get the maximum number of file descriptors allowed for the current process
 */
inline auto get_max_fds() -> int {
  struct rlimit limits{};
  if (getrlimit(RLIMIT_NOFILE, &limits) == 0) {
    return static_cast<int>(limits.rlim_cur);
  }

  std::cerr << "Error getting resource limits." << std::endl;
  return 0;
}
  
} // namespace controller
