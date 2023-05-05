#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include "logging/logger.hpp"

namespace controller {

// NOLINTNEXTLINE(cert-err58-cpp)
const std::string regulator_key = "reg";

class gdpr_regulator
{
public:
	gdpr_regulator();
  // explicit gdpr_regulator();
  // ~gdpr_regulator();

  [[nodiscard]] auto name() const -> std::string;
  [[nodiscard]] auto timestamp_thres() const -> int64_t;
  auto retrieve_logs() -> std::vector<std::string>;
  auto read_key_log(const std::string &key) -> std::vector<std::string>;
  auto read_log(const std::string &log_name) const -> std::vector<std::string>;
  static auto validate_reg_key(const controller::query &query_args, 
                               const controller::default_policy &def_policy) -> bool;
private:
  std::string m_name{};
  logger* m_history_logger;
  int64_t m_timestamp_thres;

  static auto get_filenames(const std::string &dir) -> std::vector<std::string>;
};

} // namespace controller