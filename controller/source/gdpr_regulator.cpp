#include "gdpr_regulator.hpp"
#include <chrono>

namespace controller {

/*
 * Validate that the key matches the regulator authority key
 * Currently the key is pinned in the gdpr_regulator.hpp file
 */
auto gdpr_regulator::validate_reg_key(const controller::query &query_args, 
                                      const controller::default_policy &def_policy) -> bool
{
  auto user_key = get_field_string<num_users, usr>(query_args.user_key().value_or(def_policy.user_key()), "user");
  return (user_key == regulator_key);
}

/*
 * return the directory where the logs are stored 
 * along with the filenames
 */
auto gdpr_regulator::retrieve_logs() -> std::vector<std::string> 
{
  if (!m_log_exporter) {
    return {};
  }
  return m_log_exporter->getLogFilesList();
}

/*
 * return the log entries of a specific key in human-readable form
 */
auto gdpr_regulator::read_key_log(std::string_view key, uint64_t timestamp_thres) -> std::vector<std::string> 
{
  if (!m_log_exporter) {
    return {};
  }
  pauseWorkersAndFlushLogs(); // Ensure all logs are written before reading

  return m_log_exporter->exportLogsForKey(std::string(key), timestamp_thres);
}

/*
 * return the log entries of a log in human-readable form
 */
auto gdpr_regulator::read_log(std::string_view log_name, uint64_t timestamp_thres) const -> std::vector<std::string>
{
  if (!m_log_exporter) {
    return {};
  }
  const_cast<gdpr_regulator*>(this)->pauseWorkersAndFlushLogs(); // Ensure all logs are written before reading 

  // Extract key from log_name
  std::filesystem::path logPath(log_name);
  std::string key = logPath.stem().string();
  
  return m_log_exporter->exportLogsForKey(key, timestamp_thres);
}

void gdpr_regulator::pauseWorkersAndFlushLogs()
{
  // Access logger through controller namespace
  auto* loggerInstance = controller::logger::get_instance();
  if (loggerInstance) {
    std::cout << "GDPR Regulator: Pausing workers and flushing logs..." << std::endl;
    loggerInstance->pauseWorkersAndFlushLogs();
    std::cout << "GDPR Regulator: Workers resumed, all logs flushed" << std::endl;
  } else {
    std::cout << "GDPR Regulator: No logging manager available" << std::endl;
  }
}

/*
 * Helper: return the filenames of a specific directory
 */
auto gdpr_regulator::get_filenames(std::string_view dir) -> std::vector<std::string>
{
  if (!m_log_exporter) {
    return {};
  }
  return m_log_exporter->getFilenames(std::string(dir));
}

} // namespace controller