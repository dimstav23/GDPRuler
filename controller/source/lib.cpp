#include "lib.hpp"

namespace controller {

library::library()
    : m_name {"gdpr_controller"}
{
}

// library::~library()
// {
// }

auto library::name() const -> std::string
{
    return this->m_name;
}

} // namespace controller