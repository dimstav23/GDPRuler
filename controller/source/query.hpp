#pragma once

#include <string>
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
  "delm",
  "getlogs"
};
// NOLINTEND(cert-err58-cpp)

/* class for storing query info */
class query
{
public:
	query();
  explicit query(const std::string &input);
  // overloaded constructor only for the performance test
  explicit query(std::string user_key, std::string key, std::string cmd);
  // ~query();

	[[nodiscard]] auto name() const -> std::string;

  /* private members getters */
  [[nodiscard]] auto cmd() const -> std::string;
  [[nodiscard]] auto key() const -> std::string;
  [[nodiscard]] auto value() const -> std::string;
  [[nodiscard]] auto user_key() const -> std::optional<std::string>;
  [[nodiscard]] auto purpose() const -> std::optional<std::bitset<num_purposes>>;
  [[nodiscard]] auto objection() const -> std::optional<std::bitset<num_purposes>>;
  [[nodiscard]] auto origin() const -> std::optional<std::string>;
  [[nodiscard]] auto expiration() const -> std::optional<int64_t>;
  [[nodiscard]] auto share() const -> std::optional<std::string>;
  [[nodiscard]] auto monitor() const -> std::optional<bool>;
  [[nodiscard]] auto cond_purpose() const -> std::bitset<num_purposes>;
  [[nodiscard]] auto cond_objection() const -> std::bitset<num_purposes>;
  [[nodiscard]] auto cond_origin() const -> std::string;
  [[nodiscard]] auto cond_expiration() const -> int64_t;
  [[nodiscard]] auto cond_share() const -> std::string;
  [[nodiscard]] auto cond_monitor() const -> bool;
  [[nodiscard]] auto log_key() const -> std::string;

  auto print() -> void;

private:
  
  auto process_predicate(const std::string &predicate) -> void;
  auto parse_query(const std::string& reg_query_args) -> void;
  auto parse_option(const std::string& option, const std::string& value) -> void;

  std::string m_name;

  // query data
  std::string m_cmd;
  std::string m_key;
  std::string m_value;

  // metadata to set
  std::optional<std::string> m_user_key;
  std::optional<std::bitset<num_purposes>> m_purpose;
  std::optional<std::bitset<num_purposes>> m_objection;
  std::optional<std::string> m_origin;
  std::optional<int64_t> m_expiration;
  std::optional<std::string> m_share;
  std::optional<bool> m_monitor;

  // conditional metadata
  std::bitset<num_purposes> m_cond_purpose;
  std::bitset<num_purposes> m_cond_objection;
  std::string m_cond_origin;
  int64_t m_cond_expiration;
  std::string m_cond_share;
  bool m_cond_monitor;

  // log metadata
  std::string m_log_key;
};

} // namespace controller