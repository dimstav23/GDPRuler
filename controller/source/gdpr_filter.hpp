#pragma once

#include <string>
#include <optional>
#include <sstream>

#include "gdpr_metadata.hpp"
#include "query.hpp"
#include "default_policy.hpp"

namespace controller {

/**
 * GDPR Filter Class
 * 
 * Handles two main responsibilities:
 * 1. Filter Matching: Determine if KV pairs match query filter conditions
 * 2. Access Validation: Enforce GDPR compliance for data access
 */
class gdpr_filter
{
public:
	gdpr_filter();
  explicit gdpr_filter(std::optional<std::string_view> ret_value);
  // ~gdpr_filter();

  // Filter Matching (for getm/putm operations)
  [[nodiscard]] auto matches_filter_conditions(const controller::query &query_args) const -> bool;
  // Access Validation (GDPR access control)
  [[nodiscard]] auto validate_access(const controller::query &query_args, 
                        const controller::default_policy &def_policy) const -> bool;
  // Legacy query validation
  [[nodiscard]] auto validate(const controller::query &query_args, 
                              const controller::default_policy &def_policy) const -> bool;

  [[nodiscard]] auto is_valid() const -> bool;
  [[nodiscard]] auto user_key() const -> const std::bitset<num_users>&;
  [[nodiscard]] auto purpose() const -> const std::bitset<num_purposes>&;
  [[nodiscard]] auto objection() const -> const std::bitset<num_purposes>&;
  [[nodiscard]] auto origin() const -> const std::bitset<num_origins>&;
  [[nodiscard]] auto share() const -> const std::bitset<num_users>&;
  [[nodiscard]] auto encryption() const -> bool;
  [[nodiscard]] auto expiration() const -> int64_t;
  [[nodiscard]] auto monitor() const -> bool;
  [[nodiscard]] auto check_monitoring() const -> bool;

private:
  // valid field that indicates if there is a value to be returned
  bool m_valid{false};
  
  // metadata fields
  // Note on data types: it is okay to have std::string_view for the fields
  // as the result value that they are based on outlives the gdpr_filter object
  std::bitset<num_users> m_user_key{};
  std::bitset<num_purposes> m_purpose;
  std::bitset<num_purposes> m_objection;
  std::bitset<num_origins> m_origin{};
  std::bitset<num_users> m_share{};
  bool m_encryption{false};
  int64_t m_expiration;
  bool m_monitor{false};
  
  // Validation helpers
  auto validate_ownership_or_sharing(const std::bitset<num_users>& session_key) const -> bool;
  auto validate_strict_ownership(const std::bitset<num_users>& session_key) const -> bool;
  auto validate_purpose_compliance(const std::bitset<num_purposes>& purposes) const -> bool;
  auto validate_no_objections(const std::bitset<num_purposes>& purposes) const -> bool;
  auto validate_exp_time() const -> bool;
  
  // Metadata deserialization
  void deserialize_binary_metadata(std::string_view data);
};

} // namespace controller