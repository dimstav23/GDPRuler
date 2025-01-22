#pragma once

#include <string>
#include <bitset>
#include <sstream>
#include <unordered_map>

#include "default_policy.hpp"
#include "query.hpp"

namespace controller {

/* class that combines query info and default policy info and reformats the query */
class query_rewriter
{
public:
	query_rewriter();
  /* Constructor for the PUT operation in case of the first INSERTION of a KV pair */
  explicit query_rewriter(const query &query_args, 
                          const default_policy &def_policy, 
                          std::string_view new_query_value);
  /* Constructor for the PUT operation in case of an UPDATE of a value */
  explicit query_rewriter(std::string_view res,
                          std::string_view new_query_value);
  /* Constructor for the PUTM operation */
  explicit query_rewriter(std::string_view res,
                          const query &query_args);
  // ~query_rewriter();

  [[nodiscard]] auto new_value() const -> std::string;

private:
  std::string m_new_value;
};

} // namespace controller