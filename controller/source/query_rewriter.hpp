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
  // ~query_rewriter();

	[[nodiscard]] auto name() const -> std::string;

  [[nodiscard]] auto new_key() const -> std::string;
  [[nodiscard]] auto new_value() const -> std::string;

  

private:
  std::string m_name;

  std::string m_new_key;
  std::string m_new_value;
};

} // namespace controller