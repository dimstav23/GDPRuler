#include "default_policy.hpp"

using controller::default_policy;

auto main() -> int
{
  auto const def_policy = default_policy {};

  return def_policy.name() == "gdpr_controller_default_policy" ? 0 : 1;
}
