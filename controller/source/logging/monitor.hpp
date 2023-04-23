#pragma once

#include "../gdpr_filter.hpp"
#include "../query.hpp"
#include "logger.hpp"

namespace controller {

/**
 * Class to monitor gdpr queries and log if they need to be logged.
*/
class gdpr_monitor {
public:
  gdpr_monitor(std::shared_ptr<gdpr_filter> filter, query query_args): m_filter{std::move(filter)}, m_query_args{std::move(query_args)} {
  }

  void monitor_query_attempt() {
    if (m_filter->check_monitoring()) {
      history_logger->log_attempt(m_query_args);
    }
  }

  void monitor_query_result(const bool& result, const std::string& new_val = {}) {
    if (m_filter->check_monitoring()) {
      history_logger->log_result(m_query_args, result, new_val);
    }
  }

private:
  std::shared_ptr<gdpr_filter> m_filter;

  query m_query_args;
};

} // namespace controller