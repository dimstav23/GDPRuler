#pragma once

#include <vector>
#include <string>
#include <span>

constexpr int s2ns = 1000000000;
constexpr int ns_precision = 9;

/* Parse the value corresponding to given option. Return empty string if not found. */
auto inline get_command_line_argument(const auto& args, const std::string& option) -> std::string
{
  size_t option_index = 0;
  size_t args_size = static_cast<uint>(args.size());
  for (; option_index < args_size; option_index++) {
    if (args[option_index] == option) {
      break;
    }
  }
  if (option_index + 1 < args_size) {
    return args[option_index + 1];
  }
  return {};
}