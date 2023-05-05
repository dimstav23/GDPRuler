#include "gdpr_regulator.hpp"

namespace controller {

gdpr_regulator::gdpr_regulator()
    : m_name {"gdpr_controller_gdpr_regulator"},
      m_history_logger{logger::get_instance()},
      m_timestamp_thres{std::chrono::system_clock::now().time_since_epoch().count()}
{

}

// gdpr_regulator::~gdpr_regulator()
// {
// }

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
auto gdpr_regulator::read_key_log(const std::string &key) -> std::vector<std::string> {
  // construct the filename for the given key
  std::string log_name = this->m_history_logger->get_logs_dir() + "/" +
                         key + this->m_history_logger->get_logs_extension();
  return read_log(log_name);
}

/*
 * return the log entries of a log in human-readable form
 */
auto gdpr_regulator::read_log(const std::string &log_name) const -> std::vector<std::string> {
  return controller::logger::log_decode(log_name, this->m_timestamp_thres);
}

/*
 * Helper: return the filenames of a specific directory
 */
auto gdpr_regulator::get_filenames(const std::string &dir) -> std::vector<std::string> {
  std::vector<std::string> filenames;
  std::filesystem::path logs_dir(dir);

  for (const auto& file : std::filesystem::directory_iterator(logs_dir)) {
    if (file.is_regular_file()) {
      filenames.push_back(file.path().string());
    }
  }
  return filenames;
}

auto gdpr_regulator::name() const -> std::string
{
  return this->m_name;
}

auto gdpr_regulator::timestamp_thres() const -> int64_t 
{
  return this->m_timestamp_thres;
}

} // namespace controller