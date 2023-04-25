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
  /* 
   * Generic query operation (e.g., get, delete, getm, put (on existing value))
   * Generic constructor when a value is retrieved
   * Action: Check the current gdpr metadata
   */ 
  gdpr_monitor(std::shared_ptr<gdpr_filter> filter, const query& query_args, const default_policy& def_policy): 
    m_filter{std::move(filter)}, m_query_args{query_args}, m_def_policy{def_policy}, 
    m_history_logger{logger::get_instance()}, m_monitor_needed{m_filter->check_monitoring()} 
  {
  }
  /* 
   * PUT query operation 
   * Monitor constructor when a value is put for the first time in the DB
   * Action: Check the query args & then the default policy
   */ 
  gdpr_monitor(const query& query_args, const default_policy& def_policy): 
    m_filter{nullptr}, m_query_args{query_args}, m_def_policy{def_policy}, 
    m_history_logger{logger::get_instance()}
  {
    m_monitor_needed = m_query_args.monitor().has_value() ? m_query_args.monitor().value() : m_def_policy.monitor();
  }

  // /* 
  //  * TODO: PUTM query operation 
  //  * Monitor constructor when the gdpr metadata of a value is updated 
  //  * Action: It's current gdpr metadata except from the transition from false to true based on query args
  //  */ 
  // gdpr_monitor(std::shared_ptr<gdpr_filter> filter, const query& query_args, const default_policy& def_policy): 
  //   m_filter{std::move(filter)}, m_query_args{query_args}, 
  //   m_def_policy{def_policy}, m_history_logger{logger::get_instance()} 
  // {
  //   m_monitor_needed = (!filter->check_monitoring() && m_query_args.monitor().has_value_or(false)) ?
  //                       m_query_args.monitor().value() : filter->check_monitoring();      
  // }

  void monitor_query_attempt() {
    if (m_monitor_needed) {
      m_history_logger->log_attempt(m_query_args, m_def_policy);
    }
  }

  void monitor_query_result(const bool& result, const std::string& new_val = {}) {
    if (m_monitor_needed) {
      m_history_logger->log_result(m_query_args, m_def_policy, result, new_val);
    }
  }

private:
  std::shared_ptr<gdpr_filter> m_filter;

  const query& m_query_args;

  const default_policy& m_def_policy;

  logger* m_history_logger;

  bool m_monitor_needed;

};

} // namespace controller