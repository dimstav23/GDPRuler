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

/* FILTER MATCHING
 * 
 * Purpose: Determine if a KV pair matches the query's filter conditions
 * Used in: getm/putm operations to select which KV pairs to process
 * 
 * GDPR Justification:
 * - This is NOT access control, but selection criteria
 * - Allows users to specify which data they want to operate on
 * - Similar to SQL WHERE clauses - pure filtering logic
 */
auto gdpr_filter::matches_filter_conditions(const controller::query &query_args) const -> bool
{
  if (!this->is_valid()) {
    return false;
  }
  
  // Match sessionKeyIs condition (filter by ownership)
  if (query_args.cond_user().any()) {
    const auto& filter_user_key = query_args.cond_user();
    
    // KV pair matches if the cond_user value is the owner
    bool owner_match = (this->m_user_key & filter_user_key).any();
    
    if (!owner_match) {
      #ifdef DEBUG
      std::cout << "Filter: KV pair doesn't match sessionKeyIs condition" << std::endl;
      #endif
      return false;
    }
  }
  
  // Match objPurIs condition (filter by purposes)
  if (query_args.cond_purpose().any()) {
    const auto& filter_purposes = query_args.cond_purpose();
    
    // KV pair matches if it has ALL the specified purposes
    if ((this->m_purpose & filter_purposes) != filter_purposes) {
      #ifdef DEBUG
      std::cout << "Filter: KV pair doesn't have required purposes" << std::endl;
      #endif
      return false;
    }
  }
  
  // Match objOrigIs condition (filter by origins)
  if (query_args.cond_origin().any()) {
    const auto& filter_origins = query_args.cond_origin();
    
    // KV pair matches if it has ALL the specified origins
    if ((this->m_origin & filter_origins) != filter_origins) {
      #ifdef DEBUG
      std::cout << "Filter: KV pair doesn't have required origins" << std::endl;
      #endif
      return false;
    }
  }
  
  // Match objShareIs condition (filter by sharing)
  if (query_args.cond_share().any()) {
    const auto& filter_share = query_args.cond_share();
    
    // KV pair matches if it's shared with ALL specified users
    if ((this->m_share & filter_share) != filter_share) {
      #ifdef DEBUG
      std::cout << "Filter: KV pair doesn't match sharing requirements" << std::endl;
      #endif
      return false;
    }
  }
  
  // Match objObjectionIs condition (filter by objection)
  // unlikely to be used
  if (query_args.cond_objection().any()) {
    const auto& filter_objections = query_args.cond_objection();
    
    // KV pair matches if it has ALL the specified objections
    // This filters for KV pairs that object to specific purposes
    if ((this->m_objection & filter_objections) != filter_objections) {
      #ifdef DEBUG
      std::cout << "Filter: KV pair doesn't have required objections" << std::endl;
      #endif
      return false;
    }
  }

  // Match objExpIs condition (filter by expiration time)
  if (query_args.cond_expiration() != 0) {
    const int64_t filter_expiration = query_args.cond_expiration();
    
    // Implementation: Filter KV pairs that expire before or at the specified time
    if (this->m_expiration == 0) {
      // No expiration set - doesn't match time-based filter
      #ifdef DEBUG
      std::cout << "Filter: KV pair has no expiration, doesn't match time filter" << std::endl;
      #endif
      return false;
    }
    
    if (this->m_expiration > filter_expiration) {
      // KV pair expires after the filter time - doesn't match
      #ifdef DEBUG
      std::cout << "Filter: KV pair expires too late (expiration: " << this->m_expiration 
                << ", filter: " << filter_expiration << ")" << std::endl;
      #endif
      return false;
    }
    
    #ifdef DEBUG
    std::cout << "Filter: KV pair matches expiration condition (expiration: " 
              << this->m_expiration << ", filter: " << filter_expiration << ")" << std::endl;
    #endif
  }
  
  return true; // All filter conditions matched
}

/* ACCESS VALIDATION
 * 
 * Purpose: Enforce GDPR compliance for data access
 * Used in: All operations after filtering to ensure legitimate access
 * 
 * GDPR Articles Implemented:
 * - Article #5: Purpose limitation
 * - Article #15: Right of access by users
 * - Article #17: Right to be forgotten
 * - Article #21: Right to object
 * - Article #25: Data protection by design and by default
 */
auto gdpr_filter::validate_access(const controller::query &query_args,
                                 const controller::default_policy &def_policy) const -> bool
{
  if (!this->is_valid()) {
    #ifdef DEBUG
    std::cout << "Access validation failed: Invalid KV pair metadata" << std::endl;
    #endif
    return false;
  }
  
  // Article #5: Storage limitation - check expiration
  if (!validate_exp_time()) {
    #ifdef DEBUG
    std::cout << "Access validation failed: KV pair expired" << std::endl;
    #endif
    return false;
  }
  
  // GDPR-compliant parameter determination
  const std::bitset<num_users>& effective_session_key = query_args.user_key().has_value() ? query_args.user_key().value() : def_policy.user_key();
  const std::bitset<num_purposes>& effective_purposes = query_args.cond_purpose().any() ?  query_args.cond_purpose() : def_policy.purpose();
  
  if (query_args.cmd() == "get") {
    // Article #25: Data protection by design - owner OR shared access
    bool has_access = validate_ownership_or_sharing(effective_session_key);
    bool purpose_valid = validate_purpose_compliance(effective_purposes);
    bool not_objected = validate_no_objections(effective_purposes);
    
    #ifdef DEBUG
    if (!has_access) std::cout << "Access Denied: Not owner or shared with user" << std::endl;
    if (!purpose_valid) std::cout << "Access Denied: Purpose not allowed" << std::endl;
    if (!not_objected) std::cout << "Access Denied: Purpose is objected" << std::endl;
    #endif
    
    return has_access && purpose_valid && not_objected;
  }
  else if (query_args.cmd() == "getm") {
    if (query_args.value() == "metadata") {
      // Article #15: Right of access - only data owner can see metadata
      bool is_owner = validate_strict_ownership(effective_session_key);
      
      #ifdef DEBUG
      if (!is_owner) std::cout << "Metadata access denied: Not data owner" << std::endl;
      #endif
      
      return is_owner;
    } 
    else if (query_args.value() == "data") {
      // Data access with purpose limitation
      bool has_access = validate_ownership_or_sharing(effective_session_key);
      // bool purpose_valid = validate_purpose_compliance(effective_purposes); // this can be omitted as it is checked in the filtering
      bool not_objected = validate_no_objections(effective_purposes);
      
      #ifdef DEBUG
      if (!has_access) std::cout << "Access Denied: Not owner or shared with user" << std::endl;
      // if (!purpose_valid) std::cout << "Access Denied: Purpose not allowed" << std::endl;
      if (!not_objected) std::cout << "Access Denied: Purpose is objected" << std::endl;
      #endif

      // return has_access && purpose_valid && not_objected;
      return has_access && not_objected;
    }
    else {
      std::cerr << "Invalid getm value: choose between data or metadata" << std::endl;
      return false;
    }
  }
  else if (query_args.cmd() == "putm") {
    // Article #15: Right of access - only owner can modify metadata
    // Article #25: Data protection by design - owner control
    bool is_owner = validate_strict_ownership(effective_session_key);
    
    #ifdef DEBUG
    if (!is_owner) std::cout << "Metadata update denied: Not data owner" << std::endl;
    #endif
    
    return is_owner;
  }
  else if (query_args.cmd() == "putc") {
    // Article #25: Data protection by design - complete owner control only
    bool is_owner = validate_strict_ownership(effective_session_key);
    
    #ifdef DEBUG
    if (!is_owner) std::cout << "Complete metadata control denied: Not data owner" << std::endl;
    #endif
    
    return is_owner;
  }
  else if (query_args.cmd() == "put" || query_args.cmd() == "delete") {
    // Article #17: Right to be forgotten & Article #25: owner control
    #ifdef RELAXED_OWNERSHIP
    bool is_owner = validate_ownership_or_sharing(effective_session_key); // For YCSB benchmarking: Allow shared access
    #else
    bool is_owner = validate_strict_ownership(effective_session_key);
    #endif

    #ifdef DEBUG
    if (!is_owner) std::cout << "Data modification denied: Not data owner" << std::endl;
    #endif
    
    return is_owner;
  }
  
  return false;
}

/* GDPR VALIDATION HELPERS */

// Article #25: Data protection by design - owner OR explicitly shared
auto gdpr_filter::validate_ownership_or_sharing(const std::bitset<num_users>& session_key) const -> bool
{
  return ((session_key & (this->m_user_key | this->m_share)) == session_key);
}

// Article #15: Right of access - strict ownership only
auto gdpr_filter::validate_strict_ownership(const std::bitset<num_users>& session_key) const -> bool
{
  return ((session_key & this->m_user_key) == session_key);
}

// Article #5: Purpose limitation
auto gdpr_filter::validate_purpose_compliance(const std::bitset<num_purposes>& purposes) const -> bool
{
  // User's requested purposes must be subset of data's allowed purposes
  return ((this->m_purpose & purposes) == purposes);
}

// Article #21: Right to object
auto gdpr_filter::validate_no_objections(const std::bitset<num_purposes>& purposes) const -> bool
{
  // User's requested purposes must not intersect with data's objections
  return ((this->m_objection & purposes).none());
}

// Article #5: Storage limitation
auto gdpr_filter::validate_exp_time() const -> bool
{
  if (this->m_expiration == 0) {
    return true; // No expiration set
  }
  
  int64_t current_time = std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count();
  return (current_time <= this->m_expiration);
}

auto gdpr_filter::validate(const controller::query &query_args, 
                          const controller::default_policy &def_policy) const -> bool
{
  return validate_access(query_args, def_policy);
}

/* GETTERS */
auto gdpr_filter::is_valid() const -> bool { return this->m_valid; }
auto gdpr_filter::user_key() const -> const std::bitset<num_users>& { return this->m_user_key; }
auto gdpr_filter::encryption() const -> bool { return this->m_encryption; }
auto gdpr_filter::purpose() const -> const std::bitset<num_purposes>& { return this->m_purpose; }
auto gdpr_filter::objection() const -> const std::bitset<num_purposes>& { return this->m_objection; }
auto gdpr_filter::origin() const -> const std::bitset<num_origins>& { return this->m_origin; }
auto gdpr_filter::expiration() const -> int64_t { return this->m_expiration; }
auto gdpr_filter::share() const -> const std::bitset<num_users>& { return this->m_share; }
auto gdpr_filter::monitor() const -> bool { return this->m_monitor; }

/* METADATA DESERIALIZATION */
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
  offset += header->user_bytes;
  m_purpose = convert_to_bitset<num_purposes>(data, offset, header->purpose_bytes);
  offset += header->purpose_bytes;
  m_objection = convert_to_bitset<num_purposes>(data, offset, header->purpose_bytes);
  offset += header->purpose_bytes;
  m_origin = convert_to_bitset<num_origins>(data, offset, header->origin_bytes);
  offset += header->origin_bytes;
  m_share = convert_to_bitset<num_users>(data, offset, header->user_bytes);
}

/* Check whether the query action needs to be monitored */
auto gdpr_filter::check_monitoring() const -> bool
{
  return monitor();
}

} // namespace controller