#include "query.hpp"

#include <iostream>

namespace controller {

query::query()
    : m_cond_purpose{0},
      m_cond_objection{0},
      m_cond_expiration{0},
      m_cond_monitor{false}
{
}

// overloaded constructor only for the performance test
query::query(std::string_view user_key,
             std::string_view key,
             std::string_view cmd)
    : m_cmd{cmd},
      m_key{key},
      m_user_key{user_key},
      m_cond_purpose{0},
      m_cond_objection{0},
      m_cond_expiration{0},
      m_cond_monitor{false}
{
}

query::query(std::string_view input)
    : m_cond_purpose{0},
      m_cond_objection{0},
      m_cond_expiration{0},
      m_cond_monitor{false}
{
  std::size_t start = 0;
  std::size_t pos = input.find('&');

  while (pos != std::string_view::npos) {
    // std::string predicate = input.substr(start, pos - start);
    process_predicate(input.substr(start, pos - start));
    start = pos + 1;
    pos = input.find('&', start);
  }

  // std::string last_predicate = input.substr(start);
  process_predicate(input.substr(start));
}


/**
 * Processes a predicate string by extracting the attribute and value.
 * 
 * @param predicate The predicate string in the format attribute("value").
 *
 * @note The function assumes that the predicate string is properly formatted and follows the expected pattern.
 *       If the predicate string is not in the expected format or if the attribute is not supported,
 *       the function will print an error message and exit.
 */
auto query::process_predicate(std::string_view predicate) -> void
{
  std::size_t pred_attr_start = 0;
  std::size_t pred_attr_end = predicate.find('(');
  std::string_view pred_attr = predicate.substr(pred_attr_start, pred_attr_end - pred_attr_start);

  std::size_t pred_val_start = pred_attr_end + 1;
  std::size_t pred_val_end = predicate.rfind(')');
  std::string_view pred_val = predicate.substr(pred_val_start, pred_val_end - pred_val_start);

  // Parse the options
  if (pred_attr == "query") {
    // extract command
    std::size_t query_cmd_end = pred_val.find('(');
    this->m_cmd = std::string(pred_val.substr(0, query_cmd_end));
    // Convert this->m_cmd to lowercase
    std::transform(this->m_cmd.begin(), this->m_cmd.end(), this->m_cmd.begin(), [](unsigned char cmd_char) {
      return std::tolower(cmd_char);
    });

    if (this->m_cmd == "exit") [[unlikely]] {
      // Do nothing
    } else [[likely]] {
      parse_query(pred_val);
    }
  } else {
    // indexed to remove the quotation marks from the "options" 
    pred_val = pred_val.substr(1, pred_val.length() - 2);
    parse_option(pred_attr, pred_val);
  }
}

/**
 * Extracts the key from a query string in the format "(command("key","value"))".
 * 
 * @param query The query string containing the command and key.
 * @return The extracted key from the query.
 *
 * @note The function assumes that the query string is properly formatted and follows the expected pattern.
 *       If the query string is not in the expected format, the function will print an error message and exit.
 */
static auto extract_key(std::string_view query) -> std::string_view
{
  std::size_t open_quote = query.find('"');
  std::size_t close_quote = query.find('"', open_quote + 1);
  
  if (open_quote == std::string_view::npos || close_quote == std::string_view::npos || close_quote <= open_quote) {
    std::cout << "Invalid query format: " << query << std::endl;
    return "";
  }
  
  return query.substr(open_quote + 1, close_quote - open_quote - 1);
}

auto query::parse_query(std::string_view reg_query_args) -> void
{
  // if the query is getLogs, just set the log key
  if (this->m_cmd == "getlogs") {
    this->m_log_key = std::string(extract_key(reg_query_args));
    return;
  }
  // else set the operation key and the value (if needed)
  this->m_key = std::string(extract_key(reg_query_args));
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
auto query::parse_option(std::string_view option, std::string_view value) -> void
{
  if (option == "sessionKey") {
    this->m_user_key = value;
  } 
  else if (option == "objOrig") {
    this->m_origin = value;
  } 
  else if (option == "objShare") {
    this->m_share = value;
  } 
  else if (option == "objExp") {
    this->m_expiration = stoi(std::string(value));
    // this->m_expiration = from_chars()
  } 
  else if (option == "objPur") {
    this->m_purpose.emplace();
    set_bitmap(this->m_purpose.value(), split_comma_string(value));
  } 
  else if (option == "objObjections") {
    this->m_objection.emplace();
    set_bitmap(this->m_objection.value(), split_comma_string(value));
  } 
  else if (option == "monitor") {
    this->m_monitor = str_to_bool(value);
  } 
  else if (option == "objOrigIs") {
    this->m_cond_origin = value;
  } 
  else if (option == "objShareIs") {
    this->m_cond_share = value;
  } 
  else if (option == "objExpIs") {
    this->m_cond_expiration = stoi(std::string(value));
  } 
  else if (option == "objPurIs") {
    set_bitmap(this->m_cond_purpose, split_comma_string(value));
  } 
  else if (option == "objObjectionsIs") {
    set_bitmap(this->m_cond_objection, split_comma_string(value));
  } 
  else if (option == "monitorIs") {
    this->m_cond_monitor = str_to_bool(value);
  } 
  else {
    std::string error_message = "Error: predicate " + std::string(option) + " not supported.";
    throw std::invalid_argument(error_message);
    this->m_cmd = "invalid";
  }
}

// query::~query()
// {
// }

auto query::print() -> void
{
  std::cout << this->m_cmd << " " << this->m_key << " " << this->m_value << "\n";
}

auto query::cmd() const -> std::string
{
  return this->m_cmd;
}

auto query::key() const -> std::string_view
{
  return this->m_key;
}

auto query::value() const -> std::string_view
{
  return this->m_value;
}

auto query::user_key() const -> std::optional<std::string_view>
{
  return this->m_user_key ? std::optional<std::string_view>(*this->m_user_key) : std::nullopt;
}

auto query::purpose() const -> std::optional<std::bitset<num_purposes>>
{
  return this->m_purpose;
}

auto query::objection() const -> std::optional<std::bitset<num_purposes>>
{
  return this->m_objection;
}

auto query::origin() const -> std::optional<std::string_view>
{
  return this->m_origin ? std::optional<std::string_view>(*this->m_origin) : std::nullopt;
}

auto query::expiration() const -> std::optional<int64_t>
{
  return this->m_expiration;
}

auto query::share() const -> std::optional<std::string_view>
{
  return this->m_share ? std::optional<std::string_view>(*this->m_share) : std::nullopt;
}

auto query::monitor() const -> std::optional<bool>
{
  return this->m_monitor;
}

auto query::cond_purpose() const -> std::bitset<num_purposes>
{
  return this->m_cond_purpose;
}

auto query::cond_objection() const -> std::bitset<num_purposes>
{
  return this->m_cond_objection;
}

auto query::cond_origin() const -> std::string_view
{
  return this->m_cond_origin;
}

auto query::cond_expiration() const -> int64_t
{
  return this->m_cond_expiration;
}

auto query::cond_share() const -> std::string_view
{
  return this->m_cond_share;
}

auto query::cond_monitor() const -> bool
{
  return this->m_cond_monitor;
}

auto query::log_key() const -> std::string_view
{
  return this->m_log_key;
}

} // namespace controller