#pragma once

#include <string>
#include <cstddef>
#include <mutex>
#include "absl/container/flat_hash_set.h"

namespace controller {

/**
 * Hash and equality functors for const std::string* 
 * Used to store pointers in hash sets without duplicating strings
 */
struct StringPtrHash {
  size_t operator()(const std::string* ptr) const noexcept {
    return ptr ? std::hash<std::string>{}(*ptr) : 0;
  }
};

struct StringPtrEqual {
  bool operator()(const std::string* lhs, const std::string* rhs) const noexcept {
    if (lhs == rhs) return true;
    if (!lhs || !rhs) return false;
    return *lhs == *rhs;
  }
};

// Type alias for key pointer sets
using KeyPtrSet = absl::flat_hash_set<const std::string*, StringPtrHash, StringPtrEqual>;

} // namespace controller
