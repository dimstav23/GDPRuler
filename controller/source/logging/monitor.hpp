#pragma once

#include "logger.hpp"

namespace controller {

/**
 * Class to monitor gdpr queries and log if they need to be logged.
*/
class gdpr_monitor {
public:
  /* Tags for tag dispatching to enable putm behaviour */
  struct putm_monitor_t {};
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
    m_monitor_needed = m_query_args.monitor().value_or(m_def_policy.monitor());
  }
  /* 
   * PUTM query operation 
   * Monitor constructor when the gdpr metadata of a value is updated 
   * Action: It's current gdpr metadata except from the transition from false to true based on query args
   * callable as: gdpr_monitor(filter, query_args, def_policy, controller::gdpr_monitor::putm_monitor_t{});
   */ 
  gdpr_monitor(std::shared_ptr<gdpr_filter> filter, const query& query_args,
              const default_policy& def_policy, putm_monitor_t /*unused*/): 
    m_filter{std::move(filter)}, m_query_args{query_args}, 
    m_def_policy{def_policy}, m_history_logger{logger::get_instance()} 
  {
    m_monitor_needed = (!m_filter->check_monitoring() && m_query_args.monitor().value_or(false)) ?
                        m_query_args.monitor().value() : m_filter->check_monitoring();      
  }

  void monitor_query(const bool& valid, const std::string& new_val = {}) {
    if (m_monitor_needed) {
      m_history_logger->log_encoded_query(m_query_args, m_def_policy, valid, new_val);
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