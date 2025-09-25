#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include "logging/logger.hpp"
#include "LogExporter.hpp"

namespace controller {

// set user0 to be the regulator key for proper testing
// NOLINTNEXTLINE(cert-err58-cpp)
const std::string regulator_key = "user0";

class gdpr_regulator
{
public:
	static auto get_instance() -> gdpr_regulator* {
    static gdpr_regulator regulator_instance;
    return &regulator_instance;
  }

  // Initialize the regulator with log exporter
  void initialize(std::shared_ptr<LogExporter> logExporter) {
    if (!m_initialized) {
      m_log_exporter = logExporter;
      m_initialized = true;
    }
  }
  
  void pauseWorkersAndFlushLogs();
  auto retrieve_logs() -> std::vector<std::string>;
  auto read_key_log(std::string_view key, uint64_t timestamp_thres) -> std::vector<std::string>;
  auto read_log(std::string_view log_name, uint64_t timestamp_thres) const -> std::vector<std::string>;
  static auto validate_reg_key(const controller::query &query_args, 
                               const controller::default_policy &def_policy) -> bool;
private:
  gdpr_regulator() = default;

  std::shared_ptr<LogExporter> m_log_exporter;
  bool m_initialized = false;

  auto get_filenames(std::string_view dir) -> std::vector<std::string>;
};

} // namespace controller