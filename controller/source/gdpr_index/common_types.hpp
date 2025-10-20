#pragma once

#include <string>
#include <memory>
#include <cstddef>
#include <mutex>
#include <shared_mutex>
#include "absl/container/flat_hash_set.h"

namespace controller {

// Type alias for shared string ownership
using SharedString = std::shared_ptr<std::string>;

/**
 * Hash and equality for SharedString in hash sets
 * Hashes the string content
 */
struct SharedStringHash {
  using is_transparent = void;  // Enable heterogeneous lookup
  
  size_t operator()(const SharedString& ptr) const noexcept {
    return ptr ? std::hash<std::string>{}(*ptr) : 0;
  }
  
  // For heterogeneous lookup with string_view
  size_t operator()(std::string_view sv) const noexcept {
    return std::hash<std::string_view>{}(sv);
  }
};

struct SharedStringEqual {
  using is_transparent = void;  // Enable heterogeneous lookup
  
  // SharedString vs SharedString
  bool operator()(const SharedString& lhs, const SharedString& rhs) const noexcept {
    if (lhs == rhs) return true;  // Same pointer
    if (!lhs || !rhs) return false;
    return *lhs == *rhs;
  }
  
  // SharedString vs string_view
  bool operator()(const SharedString& lhs, std::string_view rhs) const noexcept {
    return lhs && *lhs == rhs;
  }
  
  // string_view vs SharedString
  bool operator()(std::string_view lhs, const SharedString& rhs) const noexcept {
    return rhs && lhs == *rhs;
  }
  
  // string_view vs string_view (for completeness)
  bool operator()(std::string_view lhs, std::string_view rhs) const noexcept {
    return lhs == rhs;
  }
};

// Type alias for key pointer sets
using KeyPtrSet = absl::flat_hash_set<SharedString, SharedStringHash, SharedStringEqual>;

} // namespace controller
