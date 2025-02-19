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
  [[nodiscard]] auto user_key() const -> std::string_view;
  [[nodiscard]] auto encryption() const -> bool;
  [[nodiscard]] auto purpose() const -> std::bitset<num_purposes>;
  [[nodiscard]] auto objection() const -> std::bitset<num_purposes>;
  [[nodiscard]] auto origin() const -> std::string_view;
  [[nodiscard]] auto expiration() const -> int64_t;
  [[nodiscard]] auto share() const -> std::string_view;
  [[nodiscard]] auto monitor() const -> bool;

private:
  // default policy fields
  std::string m_user_key;
  bool m_encryption;
  std::bitset<num_purposes> m_purpose;
  std::bitset<num_purposes> m_objection;
  std::string m_origin;
  int64_t m_expiration;
  std::string m_share;
  bool m_monitor;

  static auto check_policy(const std::unordered_map<std::string, std::string> &map) -> void;
};

} // namespace controller