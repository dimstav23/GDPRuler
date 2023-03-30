#include "query_rewriter.hpp"

namespace controller {

query_rewriter::query_rewriter()
    : m_name {"gdpr_controller_query_rewriter"}
{
}

query_rewriter::query_rewriter(const query &query_args, 
                               const default_policy &def_policy,
                               const std::string &old_value)
    : m_name {"gdpr_controller_query_rewriter"}
{
  /* create the new value based on the query arguments and the default policy */
  std::string user_key = !query_args.user_key().empty() ? query_args.user_key() : def_policy.user_key();
  bool encryption = def_policy.encryption();
  std::bitset<num_purposes> purpose = query_args.purpose() != 0 ? query_args.purpose() : def_policy.purpose();
  std::bitset<num_purposes> objection = query_args.objection() != 0 ? query_args.objection() : def_policy.objection();
  std::string origin = !query_args.origin().empty() ? query_args.origin() : def_policy.origin();
  int64_t expiration = query_args.expiration() != 0 ? query_args.expiration() : def_policy.expiration();
  std::string share = !query_args.share().empty() ? query_args.share() : def_policy.share();
  bool monitor = query_args.monitor() ? query_args.monitor() : def_policy.monitor();

  std::stringstream prefix;
  prefix << user_key            << "|" << (encryption ? "1" : "0")        << "|"
         << purpose.to_ullong() << "|" << objection.to_ullong()           << "|"
         << origin              << "|" << get_expiration_time(expiration) << "|"
         << share               << "|" << (monitor ? "1" : "0")           << "|";
  m_new_value = prefix.str() + old_value;
}

// query_rewriter::~query_rewriter()
// {
// }

auto query_rewriter::name() const -> std::string
{
  return this->m_name;
}

auto query_rewriter::new_value() const -> std::string
{
  return this->m_new_value;
}

} // namespace controller