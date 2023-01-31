#pragma once

#include <string>

namespace controller {

class kv_interface
{
public:
  kv_interface();
  // ~kv_interface();

	[[nodiscard]] auto name() const -> std::string;

  // KV interface
  [[nodiscard]] auto kv_get() -> bool;
  [[nodiscard]] auto kv_put() -> bool;
  [[nodiscard]] auto kv_delete() -> bool;

private:
	std::string m_name;
};

} // namespace controller