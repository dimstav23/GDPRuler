#include "connection.hpp"

namespace controller {

connection::connection()
    : m_name {"gdpr_controller_connection"}
{
}

// connection::~connection()
// {
// }

auto connection::name() const -> std::string
{
  return this->m_name;
}

} // namespace controller