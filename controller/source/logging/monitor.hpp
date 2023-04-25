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
  gdpr_monitor(std::shared_ptr<gdpr_filter> filter, const query& query_args, const default_policy& def_policy): 
    m_filter{std::move(filter)}, m_query_args{query_args}, m_def_policy(def_policy), m_history_logger{logger::get_instance()} {
  }

  void monitor_query_attempt() {
    if (monitor_is_needed()) {
      m_history_logger->log_attempt(m_query_args, m_def_policy);
    }
  }

  void monitor_query_result(const bool& result, const std::string& new_val = {}) {
    if (monitor_is_needed()) {
      m_history_logger->log_result(m_query_args, m_def_policy, result, new_val);
    }
  }

private:
  std::shared_ptr<gdpr_filter> m_filter;

  const query& m_query_args;

  const default_policy& m_def_policy;

  logger* m_history_logger;

  /*
   * Criteria for monitoring:
   * 1. if the retrieved value's metadata (filter) have monitor set to true
   * 2. if there is no retrieved value & the query arguments have monitor set to true
   * 3. if there is no retrieved value & no specified query args, use the default policy
   */
  auto monitor_is_needed() -> bool {
    return m_filter->check_monitoring() || 
           (!m_filter->is_valid() && m_query_args.monitor().value_or(false)) || 
           (!m_filter->is_valid() && !m_query_args.monitor().has_value() && m_def_policy.monitor());
  }

};

} // namespace controller