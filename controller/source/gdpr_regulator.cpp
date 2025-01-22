#include "gdpr_regulator.hpp"
#include <chrono>

namespace controller {

gdpr_regulator::gdpr_regulator()
    : m_history_logger{logger::get_instance()},
      m_timestamp_thres{std::chrono::system_clock::now().time_since_epoch().count()}
{

}

// gdpr_regulator::~gdpr_regulator()
// {
// }

/*
 * Validate that the key matches the regulator authority key
 * Currently the key is pinned in the gdpr_regulator.hpp file
 */
auto gdpr_regulator::validate_reg_key(const controller::query &query_args, 
                                      const controller::default_policy &def_policy) -> bool
{
  std::string_view user_key = query_args.user_key().value_or(def_policy.user_key());
  return (user_key == regulator_key);
}

/*
 * return the directory where the logs are stored 
 * along with the filenames
 */
auto gdpr_regulator::retrieve_logs() -> std::vector<std::string> {
  return get_filenames(this->m_history_logger->get_logs_dir());
}

/*
 * return the log entries of a specific key in human-readable form
 */
auto gdpr_regulator::read_key_log(std::string_view key) -> std::vector<std::string> {
  // construct the filename for the given key
  std::string log_name = std::string(this->m_history_logger->get_logs_dir()) + "/" +
                         std::string(key) + std::string(this->m_history_logger->get_logs_extension());
  return read_log(log_name);
}

/*
 * return the log entries of a log in human-readable form
 */
auto gdpr_regulator::read_log(std::string_view log_name) const -> std::vector<std::string> {
  return this->m_history_logger->log_decode(log_name, this->m_timestamp_thres);
}

/*
 * Helper: return the filenames of a specific directory
 */
auto gdpr_regulator::get_filenames(std::string_view dir) -> std::vector<std::string> {
  std::vector<std::string> filenames;
  std::filesystem::path logs_dir(dir);

  for (const auto& file : std::filesystem::directory_iterator(logs_dir)) {
    if (file.is_regular_file()) {
      filenames.push_back(file.path().string());
    }
  }
  return filenames;
}

auto gdpr_regulator::timestamp_thres() const -> int64_t 
{
  return this->m_timestamp_thres;
}

} // namespace controller