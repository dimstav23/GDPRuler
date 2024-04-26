#include "query_rewriter.hpp"

namespace controller {

query_rewriter::query_rewriter()
    : m_name {"gdpr_controller_query_rewriter"}
{
}

/* Constructor for put query operation rewriter in case of the first INSERTION of a KV pair */
query_rewriter::query_rewriter(const query &query_args, 
                               const default_policy &def_policy,
                               const std::string &new_query_value)
    : m_name {"gdpr_controller_query_rewriter"}
{
  /* create the new value based on the query arguments and the default policy */
  std::string user_key = query_args.user_key().has_value() ? query_args.user_key().value() : def_policy.user_key();
  bool encryption = def_policy.encryption();
  std::bitset<num_purposes> purpose = query_args.purpose().has_value() ? query_args.purpose().value() : def_policy.purpose();
  std::bitset<num_purposes> objection = query_args.objection().has_value() ? query_args.objection().value() : def_policy.objection();
  std::string origin = query_args.origin().has_value() ? query_args.origin().value() : def_policy.origin();
  int64_t expiration = query_args.expiration().has_value() ? query_args.expiration().value() : def_policy.expiration();
  std::string share = query_args.share().has_value() ? query_args.share().value() : def_policy.share();
  bool monitor = query_args.monitor().has_value() ? query_args.monitor().value() : def_policy.monitor();
  
  std::stringstream prefix;
  prefix << user_key            << "|" << (encryption ? "1" : "0")        << "|"
         << purpose.to_ullong() << "|" << objection.to_ullong()           << "|"
         << origin              << "|" << get_expiration_time(expiration) << "|"
         << share               << "|" << (monitor ? "1" : "0")           << "|";
  m_new_value = prefix.str() + new_query_value;
}

/* Constructor for put query operation rewriter in case of an UPDATE of a value */
// To suppress bugprone-easily-swappable-parameters warning from clang-tidy
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
query_rewriter::query_rewriter(const std::string &res, const std::string &new_query_value)
    : m_name {"gdpr_controller_query_rewriter"}
{
  /* create the new value based on the current prefix and the new provided value */
  // Find the position of the last "|" character in the string
  size_t last_delimiter_pos = res.find_last_of('|');

  // Extract the substring up to the position of the last '|' character
  std::string metadata = res.substr(0, last_delimiter_pos);

  m_new_value = metadata + "|" + new_query_value;
}

/* Constructor for PUTM query operation rewriter */
query_rewriter::query_rewriter(const std::string &res,
                               const query &query_args)
    : m_name {"gdpr_controller_query_rewriter"}
{
  /* create the new metadata fields based on the query arguments - the rest are left intact */
  // Split the current value string into metadata fields + the value in the end
  std::istringstream iss(res);
  std::string token;
  int count = 0;
  std::stringstream prefix; // result string
  while (count < max_gdpr_field_guard && std::getline(iss, token, '|')) {
    switch (count) {
      case usr:
        prefix << query_args.user_key().value_or(token);// query_args.user_key().has_value() ? query_args.user_key().value() : token;
        break;
      case encr:
        /* encr cannot be updated during runtime */
        prefix << token;
        break;
      case pur:
        if (query_args.purpose().has_value()) {
          prefix << query_args.purpose().value().to_ullong();
        }
        else {
          prefix << token;
        }
        break;
      case obj:
        if (query_args.objection().has_value()) {
          prefix << query_args.objection().value().to_ullong();
        }
        else {
          prefix << token;
        }
        break;
      case org:
        prefix << query_args.origin().value_or(token);
        break;
      case exp:
        if (query_args.expiration().has_value()) {
          prefix << get_expiration_time(query_args.expiration().value());
        }
        else {
          prefix << token;
        }
        break;
      case shr:
        prefix << query_args.share().value_or(token);
        break;
      case log:
        if (query_args.monitor().has_value()) {
          prefix << query_args.monitor().value() ? "1" : "0";
        }
        else {
          prefix << token;
        }
        break;
      case val:
        prefix << token;
      default:
        break;
    }
    if (count != val) {
      prefix << "|";
    }
    count++;
  }

#ifdef DEBUG
  if (count != metadata_prefix_fields) {
    throw std::invalid_argument("Invalid GDPR metadata format");
  }
#endif
  m_new_value = prefix.str();
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