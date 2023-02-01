#include "query.hpp"

namespace controller {

query::query()
    : m_name {"gdpr_controller_query"},
      m_purpose{0},
      m_objection{0},
      m_expiration{0},
      m_monitor{false},
      m_cond_purpose{0},
      m_cond_objection{0},
      m_cond_expiration{0},
      m_cond_monitor{false}
{
}

query::query(const std::string &input)
    : m_name {"gdpr_controller_query"},
      m_purpose{0},
      m_objection{0},
      m_expiration{0},
      m_monitor{false},
      m_cond_purpose{0},
      m_cond_objection{0},
      m_cond_expiration{0},
      m_cond_monitor{false}
{
  std::stringstream stream_input(input);
  /* first identify the query type and the key(s)*/
  stream_input >> this->m_cmd;

  if (this->m_cmd == "getLogs") [[unlikely]] {
    stream_input >> this->m_log_count;
  }
  else if (this->m_cmd == "exit") [[unlikely]] {
    /* do nothing */
  }
  else [[likely]] {
    stream_input >> this->m_key;
    if (this->m_cmd == "put") {
      /* set the value */
      stream_input >> this->m_value;
    }

    std::string option;
    std::unordered_map<std::string, std::string> option_map;
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
        set_bitmap(this->m_purpose, split_comma_string(value));
      }
      else if (option == "-objObjections") {
        set_bitmap(this->m_objection, split_comma_string(value));
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

auto query::user_key() const -> std::string
{
  return this->m_user_key;
}

auto query::purpose() const -> std::bitset<num_purposes>
{
  return this->m_purpose;
}

auto query::objection() const -> std::bitset<num_purposes>
{
  return this->m_objection;
}

auto query::origin() const -> std::string
{
  return this->m_origin;
}

auto query::expiration() const -> int64_t
{
  return this->m_expiration;
}

auto query::share() const -> std::string
{
  return this->m_share;
}

auto query::monitor() const -> bool
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

auto query::log_count() const -> std::string
{
  return this->m_log_count;
}

} // namespace controller