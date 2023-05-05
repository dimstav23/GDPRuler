#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include "logging/logger.hpp"

namespace controller {

class gdpr_regulator
{
public:
	gdpr_regulator();
  // explicit gdpr_regulator();
  // ~gdpr_regulator();

  [[nodiscard]] auto name() const -> std::string;
  auto retrieve_logs() -> std::vector<std::string>;
  auto read_key_log(const std::string &key) -> std::vector<std::string>;
  static auto read_log(const std::string &log_name) -> std::vector<std::string>;
  
private:
  std::string m_name{};
  logger* m_history_logger;

  static auto get_filenames(const std::string &dir) -> std::vector<std::string>;
};

} // namespace controller