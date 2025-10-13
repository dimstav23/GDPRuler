#pragma once

#include "common_types.hpp"
#include "absl/container/flat_hash_map.h"
#include <vector>
#include <shared_mutex>

namespace controller {

/**
 * HashmapIndex: O(1) lookups for single-valued fields (owner, origin)
 * 
 * Use case: Owner and Origin bitmaps always have exactly 1 bit set,
 *       so they behave like a single integer value.
 * 
 * Structure: bit_position -> set of key pointers
 */
class HashmapIndex {
public:
  HashmapIndex() = default;
  
  void insert(size_t bit_position, const std::string* key_ptr);
  void remove(size_t bit_position, const std::string* key_ptr);
  std::vector<const std::string*> find(size_t bit_position) const;
  void clear();
  size_t size() const;

private:
  mutable std::shared_mutex mutex_;
  absl::flat_hash_map<size_t, KeyPtrSet> data_;
};

} // namespace controller
