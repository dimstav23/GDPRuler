#pragma once

#include <string>
#include <algorithm>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <bitset>
#include <cstring>

namespace controller {

constexpr int num_users = 128; // used for users and shared_with fields
constexpr int num_purposes = 128; // used for purposes and objections
constexpr int num_origins = 128; // used to map data origins sources
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

// Generic template for creating index maps
template<typename T>
auto create_index_map(const std::string& prefix, std::size_t count) -> std::unordered_map<std::string, std::size_t> {
  std::unordered_map<std::string, std::size_t> temp;
  for (std::size_t i = 0; i < count; i++) {
    std::string value = prefix + std::to_string(i);
    temp[value] = i;
  }
  return temp;
}

// Create static maps for core metadata field types (usr/shr and pur/obj are correlated)
// NOLINTBEGIN(cert-err58-cpp)
static const std::unordered_map<std::string, std::size_t> pur_index = 
  create_index_map<metadata_fields>("purpose", num_purposes);
static const std::unordered_map<std::string, std::size_t> usr_index = 
  create_index_map<metadata_fields>("user", num_users);
static const std::unordered_map<std::string, std::size_t> org_index = 
  create_index_map<metadata_fields>("src", num_origins);

// Generic getter functions
template<metadata_fields Field>
auto inline get_index_map() -> const std::unordered_map<std::string, std::size_t>&;

template<>
auto inline get_index_map<pur>() -> const std::unordered_map<std::string, std::size_t>& {
  return pur_index;
}

template<>
auto inline get_index_map<obj>() -> const std::unordered_map<std::string, std::size_t>& {
  return pur_index;
}

template<>
auto inline get_index_map<usr>() -> const std::unordered_map<std::string, std::size_t>& {
  return usr_index;
}

template<>
auto inline get_index_map<shr>() -> const std::unordered_map<std::string, std::size_t>& {
  return usr_index;
}

template<>
auto inline get_index_map<org>() -> const std::unordered_map<std::string, std::size_t>& {
  return org_index;
}

// Generic bitmap setter
/* 
 *  takes as arguments a bitset and a vector of strings
 *  identifies the respective bit for each key based on the defined map of metadata fields
 *  and sets the appropriate bits
 */
template<std::size_t N, metadata_fields Field>
auto inline set_bitmap(std::bitset<N> &bits, const std::vector<std::string> &bit_keys) -> void {
  const auto& index_map = get_index_map<Field>();
  for (const auto &bit_key : bit_keys) {
    auto it = index_map.find(bit_key);
    if (it != index_map.end()) {
      bits.set(it->second);
    }
  }
}

// Generic string generator
/* 
 *  takes as arguments a bitset and
 *  identifies the respective set bits and, based on the defined map of metadata fields,
 *  returns a comma separated string with the appropriate set of metadata fields
 */
template<std::size_t N, metadata_fields Field>
auto inline get_field_string(const std::bitset<N> &bits, const std::string& prefix) -> std::string {
  std::stringstream res;
  for (size_t i = 0; i < bits.size(); i++) {
    if (bits.test(i)) {
      res << prefix << i << ",";
    }
  }
  return res.str();
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

/* convert "true" -> true, "false" -> false without a copy */
auto inline str_to_bool(std::string_view str) -> bool {
  auto equals_ignore_case = [](std::string_view lhs, std::string_view rhs) -> bool {
    if (lhs.size() != rhs.size()) {
      return false;
    }
    return std::equal(lhs.begin(), lhs.end(), rhs.begin(),
                      [](char a, char b) { return std::tolower(a) == std::tolower(b); });
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
auto inline remove_gdpr_metadata(std::string value) -> std::string {
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
auto inline preserve_only_gdpr_metadata(std::string value) -> std::string {
  size_t last_delimiter_idx = value.find_last_of('|');
  if (last_delimiter_idx != std::string::npos && last_delimiter_idx + 1 < value.length())
  {
    // Erase the value and return only the GDPR metadata
    value.erase(last_delimiter_idx + 1, value.length());
  }  
  return value;
}

} // namespace controller