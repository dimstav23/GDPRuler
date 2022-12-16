#pragma once

#include <string>

namespace controller {

class kv_interface
{
public:
  kv_interface();
  // ~kv_interface();

	[[nodiscard]] auto name() const -> std::string;

private:
	std::string m_name;
};

} // namespace controller