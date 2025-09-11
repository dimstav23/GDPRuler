#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include "logging/logger.hpp"

namespace controller {

// set user0 to be the regulator key for proper testing
// NOLINTNEXTLINE(cert-err58-cpp)
const std::string regulator_key = "user0";

class gdpr_regulator
{
public:
	gdpr_regulator();
  // explicit gdpr_regulator();
  // ~gdpr_regulator();

  [[nodiscard]] auto timestamp_thres() const -> int64_t;
  auto retrieve_logs() -> std::vector<std::string>;
  auto read_key_log(std::string_view key) -> std::vector<std::string>;
  auto read_log(std::string_view log_name) const -> std::vector<std::string>;
  static auto validate_reg_key(const controller::query &query_args, 
                               const controller::default_policy &def_policy) -> bool;
private:
  logger* m_gdpr_logger;
  int64_t m_timestamp_thres;

  static auto get_filenames(std::string_view dir) -> std::vector<std::string>;
};

} // namespace controller