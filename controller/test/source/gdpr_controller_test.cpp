#include "lib.hpp"

auto main() -> int
{
  auto const lib = library {};

  return lib.name == "gdpr_controller" ? 0 : 1;
}
