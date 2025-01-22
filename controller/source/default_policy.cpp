#include "default_policy.hpp"
#include <iostream>

namespace controller {

default_policy::default_policy()
    : m_encryption{false},
      m_purpose{0},
      m_objection{0},
      m_expiration{0},
      m_monitor{false}
{
}

default_policy::default_policy(const std::string &input)
    : m_encryption{false},
      m_purpose{0},
      m_objection{0},
      m_expiration{0},
      m_monitor{false}
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
  check_policy(option_map);
  this->m_user_key = option_map["-sessionKey"];
  this->m_origin = option_map["-origin"];
  this->m_share = option_map["-objShare"];
  this->m_expiration = stoll(option_map["-expTime"]);
  this->m_encryption = str_to_bool(std::string_view(option_map["-encryption"]));
  this->m_monitor = str_to_bool(std::string_view(option_map["-monitor"]));
  set_bitmap(this->m_purpose, split_comma_string(std::string_view(option_map["-purpose"])));
  set_bitmap(this->m_objection, split_comma_string(std::string_view(option_map["-objection"])));
}

// default_policy::~default_policy()
// {
// }

/* checks that all the attributes of the default policy config are present with a value */
auto default_policy::check_policy(const std::unordered_map<std::string, std::string> &map) -> void {
  std::vector<std::string> options = {"-sessionKey", "-encryption", "-purpose", "-objection", 
                                      "-origin", "-expTime", "-objShare", "-monitor"};
  for (const auto &option : options) {
    if (!map.contains(option)) {
      std::string error_message = "Error: option " + option + " not provided.";
      throw std::invalid_argument(error_message);
    }
  }
}

auto default_policy::user_key() const -> std::string
{
  return this->m_user_key;
}

auto default_policy::encryption() const -> bool
{
  return this->m_encryption;
}

auto default_policy::purpose() const -> std::bitset<num_purposes>
{
  return this->m_purpose;
}

auto default_policy::objection() const -> std::bitset<num_purposes>
{
  return this->m_objection;
}

auto default_policy::origin() const -> std::string
{
  return this->m_origin;
}

auto default_policy::expiration() const -> int64_t
{
  return this->m_expiration;
}

auto default_policy::share() const -> std::string
{
  return this->m_share;
}

auto default_policy::monitor() const -> bool
{
  return this->m_monitor;
}


} // namespace controller