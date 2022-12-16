#pragma once

#include <string>

namespace controller {

class connection
{
public:
	connection();
  // ~connection();

	[[nodiscard]] auto name() const -> std::string;
	// send
	// receive

private:
  std::string m_name;
	// ip_addr/hostname
	// port
	// fd for the socket
};

} // namespace controller