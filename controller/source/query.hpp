#pragma once

#include <string>
#include <string_view>
#include <optional>
#include <sstream>
#include <unordered_map>

#include "gdpr_metadata.hpp"

namespace controller {

constexpr int value_size = 1024;

// NOLINTNEXTLINE(cert-err58-cpp)
static const std::string value(value_size, 'x');

/* return the dummy value */
auto inline get_value() -> std::string
{
  return value;
}

// Consts for query parsing
// NOLINTBEGIN(cert-err58-cpp)
const std::vector<std::string> policy_predicates = {
  "eq",
  "le",
  "lt",
  "ge",
  "gt",
  "sessionKey",
  "sessionKeyIs",
  "objExp",
  "objExpIs",
  "objPur",
  "objPurIs",
  "objOrig",
  "objOrigIs",
  "objShare",
  "objShareIs",
  "objObjections",
  "objObjectionsIs",
  "objOwner",
  "objOwnerIs",
  "monitor",
  "query"
};
const std::vector<std::string> query_types = {
  "put",
  "get",
  "delete",
  "putm",
  "getm",
  "getlogs"
};
// NOLINTEND(cert-err58-cpp)

/* class for storing query info */
class query
{
public:
	query();
  explicit query(std::string_view input);
  // overloaded constructor only for the performance test
  explicit query(std::string_view user_key, std::string_view key, std::string_view cmd);
  // ~query();

  /* private members getters */
  [[nodiscard]] auto cmd() const -> std::string_view;
  [[nodiscard]] auto key() const -> std::string_view;
  [[nodiscard]] auto value() const -> std::string_view;
  [[nodiscard]] auto user_key() const -> std::optional<std::string_view>;
  [[nodiscard]] auto purpose() const -> std::optional<std::bitset<num_purposes>>;
  [[nodiscard]] auto objection() const -> std::optional<std::bitset<num_purposes>>;
  [[nodiscard]] auto origin() const -> std::optional<std::string_view>;
  [[nodiscard]] auto expiration() const -> std::optional<int64_t>;
  [[nodiscard]] auto share() const -> std::optional<std::string_view>;
  [[nodiscard]] auto monitor() const -> std::optional<bool>;
  [[nodiscard]] auto cond_purpose() const -> std::bitset<num_purposes>;
  [[nodiscard]] auto cond_objection() const -> std::bitset<num_purposes>;
  [[nodiscard]] auto cond_origin() const -> std::string_view;
  [[nodiscard]] auto cond_expiration() const -> int64_t;
  [[nodiscard]] auto cond_share() const -> std::string_view;
  [[nodiscard]] auto cond_monitor() const -> bool;
  [[nodiscard]] auto log_key() const -> std::string_view;

  auto print() -> void;

private:
  
  auto process_predicate(std::string_view predicate) -> void;
  auto parse_query(std::string_view reg_query_args) -> void;
  auto parse_option(std::string_view option, std::string_view value) -> void;

  // query data
  std::string_view m_cmd;
  std::string_view m_key;
  std::string_view m_value;

  // metadata to set
  std::optional<std::string_view> m_user_key;
  std::optional<std::bitset<num_purposes>> m_purpose;
  std::optional<std::bitset<num_purposes>> m_objection;
  std::optional<std::string> m_origin;
  std::optional<int64_t> m_expiration;
  std::optional<std::string_view> m_share;
  std::optional<bool> m_monitor;

  // conditional metadata
  std::bitset<num_purposes> m_cond_purpose;
  std::bitset<num_purposes> m_cond_objection;
  std::string_view m_cond_origin;
  int64_t m_cond_expiration;
  std::string_view m_cond_share;
  bool m_cond_monitor;

  // log metadata
  std::string_view m_log_key;
};

} // namespace controller