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

auto inline split_comma_string(std::string_view str) -> std::vector<std::string> {
    std::vector<std::string> result;
    size_t start = 0;
    size_t end = str.find(',');

    while (end != std::string_view::npos) {
        result.emplace_back(str.substr(start, end - start));
        start = end + 1;
        end = str.find(',', start);
    }

    // Add the last token (or the only token if there are no commas)
    result.emplace_back(str.substr(start));

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

/* convert "true" -> true, "false" -> false */
auto inline str_to_bool(std::string_view str) -> bool {
  std::string mod_str(str);
  std::transform(mod_str.begin(), mod_str.end(), mod_str.begin(), ::tolower);
  if (mod_str == "true") {
    return true;
  }
  if (mod_str == "false") {
    return false;
  }
  throw std::runtime_error("Invalid string value");
}

/* without copy */
auto inline strview_to_bool(std::string_view str) -> bool {
    // Convert string_view to lowercase directly during comparison
    auto equals_ignore_case = [](std::string_view lhs, std::string_view rhs) -> bool {
        if (lhs.size() != rhs.size()) return false;
        for (size_t i = 0; i < lhs.size(); ++i) {
            if (std::tolower(lhs[i]) != std::tolower(rhs[i])) return false;
        }
        return true;
    };

    if (equals_ignore_case(str, "true")) {
        return true;
    }
    if (equals_ignore_case(str, "false")) {
        return false;
    }

    throw std::runtime_error("Invalid string value: " + std::string(str));
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

/**
 * Removes the metadata from the given string containing GDPR metadata and returns the actual value.
 * The input string is modified in-place.
 *
 * @param value The string containing the GDPR metadata and actual value.
 * @return The actual value after removing the metadata.
 */
auto inline remove_gdpr_metadata(std::string &value) -> std::string {
  size_t last_delimiter_idx = value.find_last_of('|');
  if (last_delimiter_idx != std::string::npos && last_delimiter_idx + 1 < value.length())
  {
    // Erase the metadata and return the actual value
    value.erase(0, last_delimiter_idx + 1);
  } 
  return value;
}

/**
 * Preserves only the GDPR metadata from the given string containing GDPR metadata.
 * The input string is modified in-place.
 *
 * @param value The string containing the GDPR metadata and the value.
 * @return The GDPR metadata.
 */
auto inline preserve_only_gdpr_metadata(std::string &value) -> std::string {
  size_t last_delimiter_idx = value.find_last_of('|');
  if (last_delimiter_idx != std::string::npos && last_delimiter_idx + 1 < value.length())
  {
    // Erase the value and return only the GDPR metadata
    value.erase(last_delimiter_idx + 1, value.length());
  }  
  return value;
}

} // namespace controller