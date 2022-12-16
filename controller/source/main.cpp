#include <iostream>
#include <string>

#include "lib.hpp"
#include "connection.hpp"
#include "kv_interface.hpp"

using controller::library;
using controller::connection;
using controller::kv_interface;

auto main() -> int
{
  auto const lib = library {};
  auto const message = "Hello from " + lib.name() + "!";
  std::cout << message << '\n';

  auto const conn = connection {};
  auto const message_conn = "Hello from " + conn.name() + "!";
  std::cout << message_conn << '\n';

  auto const kv_api = kv_interface {};
  auto const message_kv = "Hello from " + kv_api.name() + "!";
  std::cout << message_kv << '\n';
  return 0;
}
