#pragma once

#include <string>
#include <bitset>
#include <sstream>
#include <unordered_map>

#include "gdpr_metadata.hpp"

namespace controller {

constexpr int value_size = 1024;

/* construct the value */
auto inline get_value() -> std::string
{
  // Initialized upon first call to the function.
  static const std::string value(value_size, 'x');
  return value;
}

/* class for storing query info */
class query
{
public:
	query();
  explicit query(const std::string &input);
  // ~query();

	[[nodiscard]] auto name() const -> std::string;

  /* private members getters */
  [[nodiscard]] auto cmd() const -> std::string;
  [[nodiscard]] auto key() const -> std::string;
  [[nodiscard]] auto value() const -> std::string;
  [[nodiscard]] auto user_key() const -> std::string;
  [[nodiscard]] auto purpose() const -> std::bitset<num_purposes>;
  [[nodiscard]] auto objection() const -> std::bitset<num_purposes>;
  [[nodiscard]] auto origin() const -> std::string;
  [[nodiscard]] auto expiration() const -> int64_t;
  [[nodiscard]] auto share() const -> std::string;
  [[nodiscard]] auto monitor() const -> bool;
  [[nodiscard]] auto cond_user_key() const -> std::string;
  [[nodiscard]] auto cond_purpose() const -> std::bitset<num_purposes>;
  [[nodiscard]] auto cond_objection() const -> std::bitset<num_purposes>;
  [[nodiscard]] auto cond_origin() const -> std::string;
  [[nodiscard]] auto cond_expiration() const -> int64_t;
  [[nodiscard]] auto cond_share() const -> std::string;
  [[nodiscard]] auto cond_monitor() const -> bool;
  [[nodiscard]] auto log_count() const -> std::string;

  auto print() -> void;

private:
  std::string m_name;

  // query data
  std::string m_cmd;
  std::string m_key;
  std::string m_value;

  // metadata to set
  std::string m_user_key;
  std::bitset<num_purposes> m_purpose;
  std::bitset<num_purposes> m_objection;
  std::string m_origin;
  int64_t m_expiration;
  std::string m_share;
  bool m_monitor;

  // conditional metadata
  std::string m_cond_user_key;
  std::bitset<num_purposes> m_cond_purpose;
  std::bitset<num_purposes> m_cond_objection;
  std::string m_cond_origin;
  int64_t m_cond_expiration;
  std::string m_cond_share;
  bool m_cond_monitor;

  // log metadata
  std::string m_log_count;
};

} // namespace controller