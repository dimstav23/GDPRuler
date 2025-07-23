#pragma once

#include <string>
#include <sstream>
#include <unordered_map>

#include "gdpr_metadata.hpp"

namespace controller {

static constexpr const char* def_policy_prefix = "user_policy";

class default_policy
{
public:
	default_policy();
  explicit default_policy(const std::string &input);
  // ~default_policy();

  /* private members getters */
  [[nodiscard]] auto user_key() const -> std::bitset<num_users>;
  [[nodiscard]] auto encryption() const -> bool;
  [[nodiscard]] auto purpose() const -> std::bitset<num_purposes>;
  [[nodiscard]] auto objection() const -> std::bitset<num_purposes>;
  [[nodiscard]] auto origin() const -> std::bitset<num_origins>;
  [[nodiscard]] auto expiration() const -> int64_t;
  [[nodiscard]] auto share() const -> std::bitset<num_users>;
  [[nodiscard]] auto monitor() const -> bool;

private:
  // default policy fields
  std::bitset<num_users> m_user_key;
  std::bitset<num_purposes> m_purpose;
  std::bitset<num_purposes> m_objection;
  std::bitset<num_origins> m_origin;
  std::bitset<num_users> m_share;
  bool m_encryption;
  int64_t m_expiration;
  bool m_monitor;

  static auto check_policy(const std::unordered_map<std::string, std::string> &map) -> void;

  // Helper functions to get string representations of the fields
  auto purpose_string() const -> std::string;
  auto user_string() const -> std::string;
  auto objection_string() const -> std::string;
  auto origin_string() const -> std::string;
  auto share_string() const -> std::string;
};

} // namespace controller