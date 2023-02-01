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
  std::unordered_map<std::string, std::string> option_map;
  std::stringstream stream_input(input);
  std::string option;
  while (stream_input >> option) {
    if (option[0] == '-') {
      std::string value;
      stream_input >> value;
      option_map[option] = value;
    }
  }
  this->m_user_key = option_map["-sessionKey"];
  this->m_origin = option_map["-objOrig"];
  this->m_share = option_map["-objShare"];
  this->m_expiration = stoi(option_map["-objExp"]);
  this->m_monitor = str_to_bool(option_map["-monitor"]);
  set_bitmap(this->m_purpose, split_comma_string(option_map["-objPur"]));
  set_bitmap(this->m_objection, split_comma_string(option_map["-objObjections"]));
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

} // namespace controller