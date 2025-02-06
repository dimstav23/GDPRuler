#pragma once

#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>
#include <boost/algorithm/string.hpp>


/**
 * query_message is the expected message protocol sent from clients to the server.
 * 
 * The raw message contains three fields: command, key, and value.
 * is_valid field is populated after receiving and parsing the raw message.
 * 
 * Expected message protocol: "<command> <key> <value>"
 * 
 * Example query_messages         || Their meanings
 *  "del key_to_delete"           -> delete the entry with key "key_to_delete"
 *  "get key_to_get"              -> get the entry with key "key_to_get"
 *  "put key_to_put value_to_put" -> put entry with {"key_to_put": "value_to_put"}
*/
class query_message
{
public:
  query_message() = default;

  auto serialize() -> std::string
  {
    std::string result;
    result.append(m_command).append(" ").append(m_key);
    if (!m_value.empty()) {
      result.append(" ").append(m_value);
    }
    return result;
  }

  static auto deserialize(std::string_view raw_query) -> query_message
  {
    static const std::unordered_set<std::string_view> valid_query_types {
      "get", "put", "del", "getm", "putm"
    };

    query_message request;

    // Find command (first token)
    const size_t first_space = raw_query.find(' ');
    if (first_space == std::string_view::npos) [[unlikely]] {
      std::cerr << "Invalid query: missing command\n";
      return request; // invalid
    }
    request.m_command = raw_query.substr(0, first_space);

    // Command validation
    if (!valid_query_types.count(request.m_command)) [[unlikely]] {
      std::cerr << "Invalid command: " << request.m_command << '\n';
      return request;
    }

    // Find key (second token)
    size_t key_start = first_space + 1;
    if (key_start >= raw_query.size()) [[unlikely]] {
      std::cerr << "Invalid query: missing key\n";
      return request; // invalid
    }
    size_t key_end = raw_query.find(' ', key_start);
    request.m_key = raw_query.substr(key_start, key_end - key_start);

    // Handle value (remaining string)
    if (request.m_command == "put") {
      size_t value_start = key_end + 1;
      if (value_start >= raw_query.size()) [[unlikely]] {
        std::cerr << "Invalid put query: missing value\n";
        return request; // invalid
      }
      request.m_value = raw_query.substr(value_start);
    }

    request.m_is_valid = true;
    return request;
  }

  auto get_command() -> std::string_view {
    return m_command;
  }

  void set_command(std::string_view command) {
    m_command = command;
  }

  auto get_key() -> std::string_view {
    return m_key;
  }

  void set_key(std::string_view key) {
    m_key = key;
  }

  auto get_value() -> std::string_view {
    return m_value;
  }

  void set_value(std::string_view value) {
    m_value = value;
  }

  auto get_is_valid() const -> bool {
    return m_is_valid;
  }

  void set_is_valid(bool is_valid) {
    m_is_valid = is_valid;
  }
  
private:
  std::string_view m_command;
  std::string_view m_key;
  std::string_view m_value;
  bool m_is_valid {false};
};


/**
 * response_message is the expected message protocol sent from server to clients.
 * 
 * The raw message contains two fields: is_success, data.
 * is_success represents the result of an operation.
 * data represents the value retrieved in case of a successful get operation, 
 *  or the response message otherwise.
 * 
 * Expected message protocol: "<status:{1 for success, 0 for failure}>: <data>"
 * 
 * Example query_messages         || Their meanings
 *  "1"                           -> put/get/del the entry
 *  "0"                           -> operation failed
 *  "1 value_retrieved"           -> get operation succeded and value corresponding to key is "value_retrieved"
*/
class response_message
{
public:
  response_message() = default;

  response_message(bool is_success, std::string data)
      : m_is_success {is_success}
      , m_data {std::move(data)}
  {
  }

  auto serialize() -> std::string
  {
    std::string result;
    result.append(m_is_success ? "1" : "0")
          .append(m_data);
    return result;
  }

  static auto deserialize(const std::string& raw_response) -> response_message
  {
    static response_message invalid_response {/*is_success=*/false, ""};
    static std::unordered_set<char> valid_response_types {'0', '1'};
    
    if (raw_response.empty()) {
      return invalid_response;
    }

    char valid = raw_response[0];
    if (!valid_response_types.contains(valid)) {
      return invalid_response;
    }

    std::string response_data = raw_response.substr(sizeof(valid));
    return response_message {valid == '1', response_data};
  }

  auto op_is_successful() const -> bool {
    return m_is_success;
  }

  auto get_data() -> std::string {
    return m_data;
  }

private:
  bool m_is_success {false};
  std::string m_data;
};
