#include "query_rewriter.hpp"

namespace controller {

query_rewriter::query_rewriter()
    : m_name {"gdpr_controller_query_rewriter"}
{
}

// query_rewriter::~query_rewriter()
// {
// }

auto query_rewriter::name() const -> std::string
{
  return this->m_name;
}

auto query_rewriter::new_key() const -> std::string
{
  return this->m_new_key;
}

auto query_rewriter::new_value() const -> std::string
{
  return this->m_new_value;
}

} // namespace controller