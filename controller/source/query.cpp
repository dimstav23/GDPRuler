#include "query.hpp"

#include <iostream>

namespace controller {

query::query()
    : m_name {"gdpr_controller_query"},
      m_cond_purpose{0},
      m_cond_objection{0},
      m_cond_expiration{0},
      m_cond_monitor{false}
{
}

// overloaded constructor only for the performance test
query::query(std::string user_key, 
             std::string key, 
             std::string cmd)
    : m_name {"gdpr_controller_query"},
      m_cmd{std::move(cmd)},
      m_key{std::move(key)},
      m_user_key{std::move(user_key)},
      m_cond_purpose{0},
      m_cond_objection{0},
      m_cond_expiration{0},
      m_cond_monitor{false}
{
}

query::query(const std::string &input)
    : m_name {"gdpr_controller_query"},
      m_cond_purpose{0},
      m_cond_objection{0},
      m_cond_expiration{0},
      m_cond_monitor{false}
{
  std::stringstream stream_input(input);
  /* first identify the query type and the key(s)*/
  stream_input >> this->m_cmd;
  
  if (this->m_cmd == "getLogs") [[unlikely]] {
    stream_input >> this->m_log_key;
    std::string option;
    while (stream_input >> option) {
      if (option == "-sessionKeyIs") {
        std::string value;
        stream_input >> value;
        this->m_user_key = value;
      }
    }
    if (!this->m_user_key) [[unlikely]] {
      std::string error_message = "Error: getLogs query requested without providing a regulator key.";
      throw std::invalid_argument(error_message);
      this->m_cmd = "invalid";
    }
  }
  else if (this->m_cmd == "exit") [[unlikely]] {
    /* do nothing */
  }
  else [[likely]] {
    stream_input >> this->m_key;
    if (this->m_cmd == "put") {
      /* set the value */
      stream_input >> this->m_value; // to read the "VAL" placeholder
      this->m_value = get_value();  // to set the actual (dummy) value
    }

    std::string option;
    while (stream_input >> option) {
      std::string value;
      stream_input >> value;
      if (option == "-sessionKey") {
        this->m_user_key = value;
      }
      else if (option == "-objOrig") {
        this->m_origin = value;
      }
      else if (option == "-objShare") {
        this->m_share = value;
      }
      else if (option == "-objExp") {
        this->m_expiration = stoi(value);
      }
      else if (option == "-objPur") {
        this->m_purpose.emplace();
        set_bitmap(this->m_purpose.value(), split_comma_string(value));
      }
      else if (option == "-objObjections") {
        this->m_objection.emplace();
        set_bitmap(this->m_objection.value(), split_comma_string(value));
      }
      else if (option == "-monitor") {
        this->m_monitor = str_to_bool(value);
      }
      else if (option == "-sessionKeyIs") {
        this->m_cond_user_key = value;
      }
      else if (option == "-objOrigIs") {
        this->m_cond_origin = value;
      }
      else if (option == "-objShareIs") {
        this->m_cond_share = value;
      }
      else if (option == "-objExpIs") {
        this->m_cond_expiration = stoi(value);
      }
      else if (option == "-objPurIs") {
        set_bitmap(this->m_cond_purpose, split_comma_string(value));
      }
      else if (option == "-objObjectionsIs") {
        set_bitmap(this->m_cond_objection, split_comma_string(value));
      }
      else if (option == "-monitorIs") {
        this->m_cond_monitor = str_to_bool(value);
      }
      else {
        std::string error_message = "Error: predicate " + option + " not supported.";
        throw std::invalid_argument(error_message);
        this->m_cmd = "invalid";
      }
    }
  }
}

// query::~query()
// {
// }

auto query::print() -> void
{
  std::cout << this->m_cmd << " " << this->m_key << " " << this->m_value << "\n";
}

auto query::name() const -> std::string
{
  return this->m_name;
}

auto query::cmd() const -> std::string
{
  return this->m_cmd;
}

auto query::key() const -> std::string
{
  return this->m_key;
}

auto query::value() const -> std::string
{
  return this->m_value;
}

auto query::user_key() const -> std::optional<std::string>
{
  return this->m_user_key;
}

auto query::purpose() const -> std::optional<std::bitset<num_purposes>>
{
  return this->m_purpose;
}

auto query::objection() const -> std::optional<std::bitset<num_purposes>>
{
  return this->m_objection;
}

auto query::origin() const -> std::optional<std::string>
{
  return this->m_origin;
}

auto query::expiration() const -> std::optional<int64_t>
{
  return this->m_expiration;
}

auto query::share() const -> std::optional<std::string>
{
  return this->m_share;
}

auto query::monitor() const -> std::optional<bool>
{
  return this->m_monitor;
}

auto query::cond_user_key() const -> std::string
{
  return this->m_cond_user_key;
}

auto query::cond_purpose() const -> std::bitset<num_purposes>
{
  return this->m_cond_purpose;
}

auto query::cond_objection() const -> std::bitset<num_purposes>
{
  return this->m_cond_objection;
}

auto query::cond_origin() const -> std::string
{
  return this->m_cond_origin;
}

auto query::cond_expiration() const -> int64_t
{
  return this->m_cond_expiration;
}

auto query::cond_share() const -> std::string
{
  return this->m_cond_share;
}

auto query::cond_monitor() const -> bool
{
  return this->m_cond_monitor;
}

auto query::log_key() const -> std::string
{
  return this->m_log_key;
}

} // namespace controller