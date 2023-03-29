#include <iostream>
#include "gdpr_filter.hpp"

namespace controller {

gdpr_filter::gdpr_filter()
    : m_name {"gdpr_controller_gdpr_filter"}
{
}

/* deserialize the metadata from the retrieved value and place them in the fields of the filter class */
gdpr_filter::gdpr_filter(const std::optional<std::string> &value)
    : m_name {"gdpr_controller_gdpr_filter"}
{
  if (value) {
    m_valid = true;
    std::istringstream iss(*value);
    std::string token;
    std::vector<std::string> tokens;
    // retrieve the metadata fields without the actual value
    for (int i = 0; i < metadata_prefix_fields && std::getline(iss, token, '|'); i++) {
      tokens.push_back(token);
      std::cout << token << std::endl;
    }
    if (tokens.size() == metadata_prefix_fields) {
      m_user_key = tokens[0];
      m_encryption = (tokens[1] == "1");
      m_purpose = std::bitset<num_purposes>(std::stoull(tokens[2]));
      m_objection = std::bitset<num_purposes>(std::stoull(tokens[3]));
      m_origin = tokens[4];
      m_expiration = std::stoi(tokens[5]);
      m_share = tokens[6];
      m_monitor = (tokens[7] == "1");
    } 
    else {
      throw std::invalid_argument("Invalid GDPR metadata format");
    }
  }
  else {
    // no value returned for given key
    m_valid = false;
  }
}

// gdpr_filter::~gdpr_filter()
// {
// }

auto gdpr_filter::name() const -> std::string
{
  return this->m_name;
}

auto gdpr_filter::validate() const -> bool
{
  return true;
}

auto gdpr_filter::is_valid() const -> bool
{
  return this->m_valid;
}

auto gdpr_filter::user_key() const -> std::string
{
  return this->m_user_key;
}

auto gdpr_filter::encryption() const -> bool
{
  return this->m_encryption;
}

auto gdpr_filter::purpose() const -> std::bitset<num_purposes>
{
  return this->m_purpose;
}

auto gdpr_filter::objection() const -> std::bitset<num_purposes>
{
  return this->m_objection;
}

auto gdpr_filter::origin() const -> std::string
{
  return this->m_origin;
}

auto gdpr_filter::expiration() const -> int64_t
{
  return this->m_expiration;
}

auto gdpr_filter::share() const -> std::string
{
  return this->m_share;
}

auto gdpr_filter::monitor() const -> bool
{
  return this->m_monitor;
}

} // namespace controller