#pragma once

#include <string>

namespace controller {

// uint64_t pur = 0;
// uint64_t obj = 0;
// uint64_t shr = 0;
// uint64_t obj = 0;

class library
{
public:
  library();
  // ~library();

  [[nodiscard]] auto name() const -> std::string;

private:
  std::string m_name;
};

} // namespace controller