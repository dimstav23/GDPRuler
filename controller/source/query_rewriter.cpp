#include "query_rewriter.hpp"

namespace controller {

query_rewriter::query_rewriter()
{
}

/* Constructor for put query operation rewriter in case of the first INSERTION of a KV pair */
query_rewriter::query_rewriter(const query &query_args, 
                               const default_policy &def_policy,
                               std::string_view new_query_value)
{
  /* create the new value based on the query arguments and the default policy */
  /* note: string_view data type is okay as query outlives the query_rewriter */
  std::string_view user_key = query_args.user_key().value_or(def_policy.user_key());
  bool encryption = def_policy.encryption();
  std::bitset<num_purposes> purpose = query_args.purpose().value_or(def_policy.purpose());
  std::bitset<num_purposes> objection = query_args.objection().value_or(def_policy.objection());
  std::string_view origin = query_args.origin().value_or(def_policy.origin());
  int64_t expiration = query_args.expiration().value_or(def_policy.expiration());
  std::string_view share = query_args.share().value_or(def_policy.share());
  bool monitor = query_args.monitor().value_or(def_policy.monitor());
  
  // Pre-calculate the size of the final string
  size_t delimiters = (max_gdpr_field_guard - 1) * sizeof(char);

  size_t prefix_size = /* user key */   user_key.size()            + /* encryption */  sizeof(char)               +
                       /* purpose */    sizeof(unsigned long long) + /* objection */   sizeof(unsigned long long) +
                       /* origin */     origin.size()              + /* expiration */  sizeof(int64_t)            +
                       /* share */      share.size()               + /* monitor */     sizeof(char)               +
                       /* delimiters */ delimiters;

  // Reserve space for the entire string
  m_new_value.reserve(prefix_size + new_query_value.size());

  // Construct the string directly
  m_new_value.append(user_key).append(1, '|');
  m_new_value.append(encryption ? "1|" : "0|");
  m_new_value.append(std::to_string(purpose.to_ullong())).append(1, '|');
  m_new_value.append(std::to_string(objection.to_ullong())).append(1, '|');
  m_new_value.append(origin).append(1, '|');
  m_new_value.append(std::to_string(get_expiration_time(expiration))).append(1, '|');
  m_new_value.append(share).append(1, '|');
  m_new_value.append(monitor ? "1|" : "0|");
  m_new_value.append(new_query_value);
}

/* Constructor for put query operation rewriter in case of an UPDATE of a value */
// To suppress bugprone-easily-swappable-parameters warning from clang-tidy
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
query_rewriter::query_rewriter(std::string_view res, std::string_view new_query_value)
{
  /* create the new value based on the current prefix and the new provided value */
  // Find the position of the last "|" character in the string
  size_t last_delimiter_pos = res.find_last_of('|');

  // Extract the substring up to the position of the last '|' character
  auto metadata = res.substr(0, last_delimiter_pos);

  m_new_value = std::string(metadata) + "|" + std::string(new_query_value);
}

/* Constructor for PUTM query operation rewriter */
query_rewriter::query_rewriter(std::string_view res,
                               const query &query_args)
{
  /* create the new metadata fields based on the query arguments - the rest are left intact */
  // Split the current value string into metadata fields + the value in the end
  std::string new_value;
  new_value.reserve(res.length());  // Pre-allocate space

  size_t start = 0;
  size_t end = 0;
  int count = 0;

  while (count < max_gdpr_field_guard && (end = res.find('|', start)) != std::string_view::npos) {
    std::string_view token = res.substr(start, end - start);

    switch (count) {
      case usr:
        new_value.append(query_args.user_key().value_or(token));
        break;
      case encr:
        new_value.append(token);
        break;
      case pur:
        if (query_args.purpose().has_value()) {
          new_value.append(std::to_string(query_args.purpose().value().to_ullong()));
        } else {
          new_value.append(token);
        }
        break;
      case obj:
        if (query_args.objection().has_value()) {
          new_value.append(std::to_string(query_args.objection().value().to_ullong()));
        } else {
          new_value.append(token);
        }
        break;
      case org:
        new_value.append(query_args.origin().value_or(token));
        break;
      case exp:
        if (query_args.expiration().has_value()) {
          new_value.append(std::to_string(get_expiration_time(query_args.expiration().value())));
        } else {
          new_value.append(token);
        }
        break;
      case shr:
        new_value.append(query_args.share().value_or(token));
        break;
      case log:
        if (query_args.monitor().has_value()) {
          new_value.append(query_args.monitor().value() ? "1" : "0");
        } else {
          new_value.append(token);
        }
        break;
      case val:
        new_value.append(token);
        break;
      default:
        break;
    }
    if (count != val) {
      new_value.append(1, '|');
    }
    start = end + 1;
    count++;
  }

  // Append the last token (value)
  if (start < res.length()) {
    new_value.append(res.substr(start));
  }

#ifdef DEBUG
  if (count != metadata_prefix_fields) {
    throw std::invalid_argument("Invalid GDPR metadata format");
  }
#endif

  m_new_value = std::move(new_value);
}

// query_rewriter::~query_rewriter()
// {
// }

auto query_rewriter::new_value() const -> std::string
{
  return this->m_new_value;
}

} // namespace controller