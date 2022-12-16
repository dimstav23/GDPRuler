#pragma once

#include <string>

namespace controller {

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