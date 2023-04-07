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
    int count = 0;
    // retrieve the metadata fields without the actual value
    while (count < metadata_prefix_fields && std::getline(iss, token, '|')) {
      // std::cout << count << " " << token << std::endl;
      switch (count) {
        case usr: 
          m_user_key = token;
          break;
        case encr:
          m_encryption = (token == "1");
          break;
        case pur:
          m_purpose = std::bitset<num_purposes>(std::stoull(token));
          break;
        case obj:
          m_objection = std::bitset<num_purposes>(std::stoull(token));
          break;
        case org:
          m_origin = token;
          break;
        case exp:
          m_expiration = std::stoi(token);
          break;
        case shr:
          m_share = token;
          break;
        case log:
          m_monitor = (token == "1");
          break;
        default:
          break;
      }
      count++;
    }
    if (count != metadata_prefix_fields) {
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
    #ifndef NDEBUG
    std::cout << "no value returned by the query" << std::endl;
    #endif
    return false;
  }
  if (!validate_session_key(query_args.cond_user_key(), def_policy.user_key())) {
    // if no user is specified in the query, 
    // choose the current user to check if he/she
    // is the owner or the KV pair is shared w/ him/her
    #ifndef NDEBUG
    std::cout << "client key not in the owner/share groups of the KV pair" << std::endl;
    #endif
    return false;
  }
  if (!validate_pur(query_args.cond_purpose(), def_policy.purpose())) {
    // query purposes are not in the KV purposes list 
    #ifndef NDEBUG
    std::cout << "query purposes not in the allowed purposes of use of the KV pair" << std::endl;
    #endif
    return false;
  }
  if (!validate_obj(query_args.cond_purpose(), def_policy.purpose())) {
    // query purposes are in the KV objection list 
    #ifndef NDEBUG
    std::cout << "query purposes in the objections of the KV pair" << std::endl;
    #endif
    return false;
  }
  // if (!validate_org(query_args.cond_origin(), def_policy.origin())) {
  //   // query purposes are in the KV objection list 
  //   #ifndef NDEBUG
  //   std::cout << "query origin requirement different than the KV pair" << std::endl;
  //   #endif
  //   return false;
  // }
  if (!validate_exp_time()) {
    // value expired
    // TODO: delete the value from the DB
    #ifndef NDEBUG
    std::cout << "expired KV pair" << std::endl;
    #endif
    return false;
  }
  if (check_monitoring()) {
    // TODO: perform logging of the operation
    #ifndef NDEBUG
    std::cout << "Monitor required" << std::endl;
    #endif
  }
  return true;
}

/* Validate that the user session key belongs to the owner or the share_with set */
auto gdpr_filter::validate_session_key(const std::string &query_user_key,
                                        const std::string &def_user_key) const -> bool
{
  std::string user_key = query_user_key.empty() ? def_user_key : query_user_key;
  // Check if the user that requests the data is the owner (likely)
  if (user_key == this->user_key()) {
    return true;
  }
  
  // If the user is not the owner, check if the data is shared with the client-user
  // Check if share user string contains user_key as a sub-token separated by commas
  size_t start = 0;
  size_t end = std::string::npos;
  while ((end = this->share().find(',', start)) != std::string::npos) {
    if (user_key == this->share().substr(start, end - start)) {
      return true;
    }
    start = end + 1;
  }

  // Check if the user key matches the last user in the shared users string
  // if not, the function will return false
  return (user_key == this->share().substr(start));
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

// /* Validate that the requested query origin is the same with the KV pair */
// auto gdpr_filter::validate_org(const std::string &query_org,
//                                 const std::string &def_org) const -> bool
// {
//   std::string origin = query_org.empty() ? def_org : query_org;
//   // Check if the origins match
//   return (origin == this->origin());
// }

/* Validate that the KV pair is not expired */
auto gdpr_filter::validate_exp_time() const -> bool
{
  // if no expiration time has been set
  if (this->expiration() == 0) {
    return true;
  }
  int64_t current_time = std::chrono::duration_cast<std::chrono::seconds>(
                          std::chrono::system_clock::now().time_since_epoch()
                          ).count();
  return (current_time <= this->expiration());
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