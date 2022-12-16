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

} // namespace controller