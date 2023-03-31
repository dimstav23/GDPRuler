#include <iostream>
#include "gdpr_filter.hpp"

namespace controller {

/* deserialize the metadata from the retrieved value and place them in the fields of the filter class */
gdpr_filter::gdpr_filter(const std::optional<std::string> &ret_value)
    : m_name {"gdpr_controller_gdpr_filter"}
{
  if (ret_value) {
    m_valid = true;
    std::istringstream iss(*ret_value);
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

/* Perform the validation checks for the gdpr metadata */
auto gdpr_filter::validate(const controller::query &query_args, 
                            const controller::default_policy &def_policy) const -> bool
{
  if (!this->is_valid()) {
    // no value found for the query key
    return false;
  }
  if (!validate_session_key(def_policy.user_key())) {
    // current user is not the owner or the KV pair is not shared w/ him/her
    return false;
  }
  if (!validate_pur(query_args.cond_purpose(), def_policy.purpose())) {
    // query purposes are not in the KV purposes list 
    return false;
  }
  if (!validate_obj(query_args.cond_purpose(), def_policy.purpose())) {
    // query purposes are in the KV objection list 
    return false;
  }
  if (!validate_exp_time()) {
    // value expired
    // TODO: delete the value from the DB
    return false;
  }
  if (check_monitoring()) {
    // TODO: perform logging of the operation
  }
  return true;
}

/* Validate that the user session key belongs to the owner or the share_with set */
auto gdpr_filter::validate_session_key(const std::string &user_key) const -> bool
{
  // Check if the user that requests the data is the owner (likely)
  if (user_key == this->user_key()) {
    return true;
  }
  // If the user is not the owner, check if the data is shared with the client-user
  // Check if the user key matches the first user in the shared users string
  if (user_key == this->share().substr(0, user_key.length())) {
    return true;
  }

  // Iterate through the shared string looking for commas
  std::size_t start = user_key.length() + 1;
  std::size_t end = this->share().find(',', start);
  while (end != std::string::npos) {
    // Check if the user key matches the next user in the shared users string
    if (user_key == this->share().substr(start, end - start)) {
      return true;
    }
    start = end + 1;
    end = this->share().find(',', start);
  }

  // Check if the user key matches the last user in the shared users string
  if (user_key == this->share().substr(start)) {
    return true;
  }

  // If we get here, the user key was not found in the shared users string
  return false;
}

/* Validate that the purpose of the query is indeed in the allowed purposes */
auto gdpr_filter::validate_pur(const std::bitset<num_purposes> &query_pur,
                                const std::bitset<num_purposes> &def_pur) const -> bool
{
  if (query_pur.any()) {
    // the query purposes override the defaults
    return (this->purpose() & query_pur) == query_pur;
  }

  // if no query purposes are given, use the defaults of the client session
  return (this->purpose() & def_pur) == def_pur;
}

/* Validate that the purpose of the query is not in the obejction list */
auto gdpr_filter::validate_obj(const std::bitset<num_purposes> &query_pur,
                                const std::bitset<num_purposes> &def_pur) const -> bool
{
  if (query_pur.any()) {
    // the query purposes override the defaults
    return ((this->objection() & query_pur) == 0);
  }
  
  // if no query purposes are given, use the defaults of the client session
  return ((this->objection() & def_pur) == 0);
}

/* Validate that the KV pair is not expired */
auto gdpr_filter::validate_exp_time() const -> bool
{
  int64_t current_time = std::chrono::duration_cast<std::chrono::seconds>(
                          std::chrono::system_clock::now().time_since_epoch()
                          ).count();
  return (current_time <= expiration());
}

/* Check whether the query action needs to be monitored */
auto gdpr_filter::check_monitoring() const -> bool
{
  return monitor();
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