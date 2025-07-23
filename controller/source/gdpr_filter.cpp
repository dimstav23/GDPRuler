#include <iostream>
#include "gdpr_filter.hpp"

namespace controller {

gdpr_filter::gdpr_filter()
    : m_valid{true},
      m_encryption{false},
      m_purpose{0},
      m_user_key{0},
      m_objection{0},
      m_origin{0},
      m_expiration{0},
      m_share{0},
      m_monitor{false}
{
}

/* deserialize the metadata from the retrieved value and place them in the fields of the filter class */
gdpr_filter::gdpr_filter(std::optional<std::string_view> ret_value)
    : m_valid{true},
      m_encryption{false},
      m_purpose{0},
      m_user_key{0},
      m_objection{0},
      m_origin{0},
      m_expiration{0},
      m_share{0},
      m_monitor{false}
{
  if (ret_value && !ret_value->empty()) {
    try {
      deserialize_binary_metadata(*ret_value);
      m_valid = true;
    } catch (const std::exception& e) {
      #ifdef DEBUG
      std::cout << "Failed to deserialize binary data: " << e.what() << std::endl;
      #endif
      m_valid = false;
    }
  }
}

/* Perform the validation checks for the gdpr metadata */
auto gdpr_filter::validate(const controller::query &query_args, 
                            const controller::default_policy &def_policy) const -> bool
{
  if (!this->is_valid()) {
    // no value found for the query key
    #ifdef DEBUG
    std::cout << "no value returned by the query" << std::endl;
    #endif
    return false;
  }
  if (!validate_session_key(query_args.user_key(), def_policy.user_key())) {
    // if no user is specified in the query, 
    // choose the current user to check if he/she
    // is the owner or the KV pair is shared w/ him/her
    #ifdef DEBUG
    std::cout << "client key not in the owner/share groups of the KV pair" << std::endl;
    #endif
    return false;
  }
  if (!validate_pur(query_args.cond_purpose(), def_policy.purpose())) {
    // query purposes are not in the KV purposes list 
    #ifdef DEBUG
    std::cout << "query purposes not in the allowed purposes of use of the KV pair" << std::endl;
    #endif
    return false;
  }
  if (!validate_obj(query_args.cond_purpose(), def_policy.purpose())) {
    // query purposes are in the KV objection list 
    #ifdef DEBUG
    std::cout << "query purposes in the objections of the KV pair" << std::endl;
    #endif
    return false;
  }
  if (!validate_exp_time()) {
    // value expired
    // TODO: delete the value from the DB
    #ifdef DEBUG
    std::cout << "expired KV pair" << std::endl;
    #endif
    return false;
  }
  // if (check_monitoring()) {
  //   // TODO: perform logging of the operation
  //   #ifdef DEBUG
  //   std::cout << "Monitor required" << std::endl;
  //   #endif
  // }
  return true;
}

/* Validate that the user session key belongs to the owner or the share_with set */
auto gdpr_filter::validate_session_key(const std::optional<std::bitset<num_users>> &query_user_key,
                                       const std::bitset<num_users> &def_user_key) const -> bool
{
  const std::bitset<num_users>& user_key = query_user_key.has_value() ? query_user_key.value() : def_user_key;
  // Check if the user that requests the data is the owner (likely)
  // or if the data is shared with the client-user
  return ((user_key & (this->user_key() | this->share())) == user_key);
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

auto gdpr_filter::user_key() const -> const std::bitset<num_users>&
{
  return this->m_user_key;
}

auto gdpr_filter::encryption() const -> bool
{
  return this->m_encryption;
}

auto gdpr_filter::purpose() const -> const std::bitset<num_purposes>&
{
  return this->m_purpose;
}

auto gdpr_filter::objection() const -> const std::bitset<num_purposes>&
{
  return this->m_objection;
}

auto gdpr_filter::origin() const -> const std::bitset<num_origins>&
{
  return this->m_origin;
}

auto gdpr_filter::expiration() const -> int64_t
{
  return this->m_expiration;
}

auto gdpr_filter::share() const -> const std::bitset<num_users>&
{
  return this->m_share;
}

auto gdpr_filter::monitor() const -> bool
{
  return this->m_monitor;
}

auto gdpr_filter::deserialize_binary_metadata(std::string_view data) -> void {
  if (data.size() < sizeof(metadata_header)) {
    throw std::invalid_argument("Invalid binary data size - too small for header");
  }

  size_t offset = 0;
  
  // Read header
  const metadata_header* header = reinterpret_cast<const metadata_header*>(data.data());
  offset += sizeof(metadata_header);
  
  // Validate we have enough data
  size_t expected_size = sizeof(metadata_header) + header->user_bytes + 
                          header->purpose_bytes + header->purpose_bytes + 
                          header->origin_bytes + header->user_bytes;

  if (data.size() < expected_size) {
    throw std::invalid_argument("Invalid binary data size - insufficient metadata");
  }
  
  // Extract flags and expiration time
  m_encryption = (header->flags & 1) != 0;
  m_monitor = (header->flags & 2) != 0;
  m_expiration = header->expiration_time;
  
  // Convert metadata to bitsets
  m_user_key = convert_to_bitset<num_users>(data, offset, header->user_bytes);
  m_purpose = convert_to_bitset<num_purposes>(data, offset, header->purpose_bytes);
  m_objection = convert_to_bitset<num_purposes>(data, offset, header->purpose_bytes);
  m_origin = convert_to_bitset<num_origins>(data, offset, header->origin_bytes);
  m_share = convert_to_bitset<num_users>(data, offset, header->user_bytes);
}

} // namespace controller