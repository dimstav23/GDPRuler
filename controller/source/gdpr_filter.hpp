#pragma once

#include <string>
#include <optional>
#include <sstream>

#include "gdpr_metadata.hpp"
#include "query.hpp"
#include "default_policy.hpp"

namespace controller {

/* class that performs the check of the gdpr metadata */
class gdpr_filter
{
public:
	gdpr_filter();
  explicit gdpr_filter(std::optional<std::string_view> ret_value);
  // ~gdpr_filter();

  [[nodiscard]] auto is_valid() const -> bool;

  [[nodiscard]] auto user_key() const -> std::bitset<num_users>;
  [[nodiscard]] auto purpose() const -> std::bitset<num_purposes>;
  [[nodiscard]] auto objection() const -> std::bitset<num_purposes>;
  [[nodiscard]] auto origin() const -> std::bitset<num_origins>;
  [[nodiscard]] auto share() const -> std::bitset<num_users>;
  [[nodiscard]] auto encryption() const -> bool;
  [[nodiscard]] auto expiration() const -> int64_t;
  [[nodiscard]] auto monitor() const -> bool;

  [[nodiscard]] auto validate(const controller::query &query_args, 
                              const controller::default_policy &def_policy) const -> bool;
  [[nodiscard]] auto validate_session_key(const std::optional<std::bitset<num_users>> &query_user_key,
                                          const std::bitset<num_users> &def_user_key) const -> bool;
  // [[nodiscard]] auto validate_shr(const std::bitset<num_users> &query_shr,
  //                                 const std::bitset<num_users> &def_shr) const -> bool;
  // [[nodiscard]] auto validate_org(const std::bitset<num_origins> &query_org,
  //                                 const std::bitset<num_origins> &def_org) const -> bool;
  [[nodiscard]] auto validate_pur(const std::bitset<num_purposes> &query_pur,
                                  const std::bitset<num_purposes> &def_pur) const -> bool;
  [[nodiscard]] auto validate_obj(const std::bitset<num_purposes> &query_pur,
                                  const std::bitset<num_purposes> &def_pur) const -> bool;
  [[nodiscard]] auto validate_exp_time() const -> bool;
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
};

} // namespace controller