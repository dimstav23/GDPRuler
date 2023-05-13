#pragma once

#include <filesystem>
#include <cstring>

#include "../common.hpp"
#include "../gdpr_metadata.hpp"

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
inline auto gdpr_metadata_fmt(const std::string &value_str) -> std::string {
  /* Value has already been checked not to be empty */
  std::istringstream iss(value_str);
  std::string token;
  std::stringstream res;
  int count = 0;
  // retrieve the metadata fields and place them in a string
  while (count < max_gdpr_field_guard && std::getline(iss, token, '|')) {
    switch (count) {
      case usr: 
        res << "User/Owner: " << token << ", ";
        break;
      case encr:
        res << "Encryption enabled: " << bool_to_str(token == "1") << ", ";
        break;
      case pur:
        {
          auto purposes = std::bitset<num_purposes>(std::stoull(token));
          // ending comma is included in get_purposes_string()
          res << "Purposes: " << get_purposes_string(purposes) << " ";
          break;
        }
      case obj:
        {
          auto objections = std::bitset<num_purposes>(std::stoull(token));
          // ending comma is included in get_purposes_string()
          res << "Objections: " << get_purposes_string(objections) << " ";
          break;
        }
      case org:
        res << "Data origin: " << token << ", ";
        break;
      case exp:
        {
          std::string expire_time = token == "0" ? 
                                    "none" : timestamp_to_datetime(std::stoi(token));
          res << "Expiration time: " << expire_time << ", ";
          break;
        }
      case shr:
        res << "Shared with: " << token << ", ";
        break;
      case log:
        res << "Log enabled: " << bool_to_str(token == "1") << ", ";
        break;
      case val: // it's the last field, don't add a comma
        res << "Value: " << token;
        break;
      default:
        break;
    }
    count++;
  }
  if (count != max_gdpr_field_guard) {
    throw std::invalid_argument("Invalid GDPR metadata format in the logs");
  }

  return res.str();
}
  
} // namespace controller
