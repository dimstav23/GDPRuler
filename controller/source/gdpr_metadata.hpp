#pragma once

#include <string>
#include <algorithm>
#include <unordered_map>

namespace controller {

constexpr int num_users = 64;
constexpr int num_purposes = 64;

/* initialize the mapping between purposes and indexes in bitmap*/
auto inline get_pur() -> std::unordered_map<std::string, std::size_t>
{
  // Initialized upon first call to the function.
  static const std::unordered_map<std::string, std::size_t> pur_index = []() {
    std::unordered_map<std::string, std::size_t> temp;
    for (std::size_t i = 0; i < num_purposes; i++) {
      std::string value = "purpose" + std::to_string(i);
      temp[value] = i;
    }
    return temp;
  }();

  return pur_index;
}

auto inline split_comma_string(const std::string &str) -> std::vector<std::string> {
  std::vector<std::string> result;
  std::istringstream sstream(str);
  std::string token;

  while (std::getline(sstream, token, ',')) {
    result.push_back(token);
  }

  return result;
}

/* convert "true" -> true, "false" -> false */
auto inline str_to_bool(const std::string& str) -> bool {
  std::string mod_str = str;
  std::transform(mod_str.begin(), mod_str.end(), mod_str.begin(), ::tolower);
  if (mod_str == "true") {
    return true;
  }
  if (mod_str == "false") {
    return false;
  }
  throw std::runtime_error("Invalid string value");
}

} // namespace controller