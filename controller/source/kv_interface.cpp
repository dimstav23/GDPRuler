#include "kv_interface.hpp"

namespace controller {

kv_interface::kv_interface()
  : m_name {"gdpr_controller_kv_interface"}
{
}

// kv_interface::~kv_interface()
// {
// }

auto kv_interface::name() const -> std::string
{
  return this->m_name;
}

auto kv_interface::kv_get() -> bool
{
  return true;
}

auto kv_interface::kv_put() -> bool
{
  return true;
}

auto kv_interface::kv_delete() -> bool
{
  return true;
}

} // namespace controller