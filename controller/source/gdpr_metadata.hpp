#pragma once

#include <string>
#include <algorithm>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <bitset>

namespace controller {

constexpr int num_users = 64;
constexpr int num_purposes = 64;
constexpr int metadata_prefix_fields = 8;

enum metadata_fields {
  usr,
  encr,
  pur,
  obj,
  org,
  exp,
  shr,
  log,
  val,
  max_gdpr_field_guard
};

// NOLINTBEGIN(cert-err58-cpp)
static const std::unordered_map<std::string, std::size_t> pur_index = []() {
    std::unordered_map<std::string, std::size_t> temp;
    for (std::size_t i = 0; i < num_purposes; i++) {
      std::string value = "purpose" + std::to_string(i);
      temp[value] = i;
    }
    return temp;
  }();
// NOLINTEND(cert-err58-cpp)

/* return the mapping between purposes and indexes in bitmap*/
auto inline get_pur() -> std::unordered_map<std::string, std::size_t>
{
  return pur_index;
}

/* 
 *  takes as arguments a bitset and a vector of strings
 *  identifies the respective bit for each key based on the defined map of purposes
 *  and sets the appropriate bits
 */
template<std::size_t N>
auto inline set_bitmap(std::bitset<N> &bits, const std::vector<std::string> &bit_keys) -> void {
  std::size_t index = 0;
  for (const auto &bit_key : bit_keys) {
    index = get_pur()[bit_key];
    bits.set(index);
  }
}

/* 
 *  takes as arguments a bitset and
 *  identifies the respective set bits and, based on the defined map of purposes,
 *  returns a comma separated string with the appropriate set of purposes
 */
template<std::size_t N>
auto inline get_purposes_string(std::bitset<N> &bits) -> std::string {
  // Iterate over the bits and check each one
  std::stringstream res;
  for (size_t i = 0; i < bits.size(); i++) {
    if (bits.test(i)) {
      // The i-th bit is set
      res << "purpose" << i << ",";
    }
  }
  return res.str();
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

/* convert true -> "true", false -> "false" */
inline auto bool_to_str(bool value) -> std::string {
  return value ? "true" : "false";
}

/* convert integer (for expiration time) to an actual expiration date in seconds */
auto inline get_expiration_time(int64_t secs_from_now) -> int64_t {
  // if no expiration time has been set, just return 0
  if (secs_from_now == 0) {
    return 0;
  }
  auto expiration_time = std::chrono::system_clock::now() + std::chrono::seconds(secs_from_now);
  return std::chrono::duration_cast<std::chrono::seconds>(expiration_time.time_since_epoch()).count();
}

} // namespace controller