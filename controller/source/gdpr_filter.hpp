#pragma once

#include <string>
#include <optional>
#include <sstream>
#include <bitset>

#include "gdpr_metadata.hpp"

namespace controller {

/* class that performs the check of the gdpr metadata */
class gdpr_filter
{
public:
	gdpr_filter();
  explicit gdpr_filter(const std::optional<std::string> &value);
  // ~gdpr_filter();

	[[nodiscard]] auto name() const -> std::string;

  [[nodiscard]] auto is_valid() const -> bool;

  [[nodiscard]] auto user_key() const -> std::string;
  [[nodiscard]] auto encryption() const -> bool;
  [[nodiscard]] auto purpose() const -> std::bitset<num_purposes>;
  [[nodiscard]] auto objection() const -> std::bitset<num_purposes>;
  [[nodiscard]] auto origin() const -> std::string;
  [[nodiscard]] auto expiration() const -> int64_t;
  [[nodiscard]] auto share() const -> std::string;
  [[nodiscard]] auto monitor() const -> bool;

  [[nodiscard]] auto validate() const -> bool;

private:
  std::string m_name;

  // valid field that indicates if there is a value to be returned
  bool m_valid;
  
  // metadata fields
  std::string m_user_key;
  bool m_encryption;
  std::bitset<num_purposes> m_purpose;
  std::bitset<num_purposes> m_objection;
  std::string m_origin;
  int64_t m_expiration;
  std::string m_share;
  bool m_monitor;
};

} // namespace controller