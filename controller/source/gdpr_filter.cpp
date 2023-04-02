#include <iostream>
#include "gdpr_filter.hpp"

namespace controller {

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
      // std::cout << i << " " << token << std::endl;
    }
    if (tokens.size() == metadata_prefix_fields) {
      m_user_key = tokens[usr];
      m_encryption = (tokens[encr] == "1");
      m_purpose = std::bitset<num_purposes>(std::stoull(tokens[pur]));
      m_objection = std::bitset<num_purposes>(std::stoull(tokens[obj]));
      m_origin = tokens[org];
      m_expiration = std::stoi(tokens[exp]);
      m_share = tokens[shr];
      m_monitor = (tokens[log] == "1");
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
  return this->m_valid;
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