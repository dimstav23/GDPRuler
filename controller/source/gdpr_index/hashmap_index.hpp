#pragma once

#include "common_types.hpp"
#include "absl/container/flat_hash_map.h"
#include <vector>

namespace controller {

class HashmapIndex {
public:
  HashmapIndex() = default;
  
  // Now takes/returns SharedString instead of raw pointers
  void insert(size_t bit_position, const SharedString& key_ptr);
  void remove(size_t bit_position, const SharedString& key_ptr);
  std::vector<SharedString> find(size_t bit_position) const;  // Returns shared_ptr!
  void clear();
  size_t size() const;

private:
  mutable std::shared_mutex mutex_;
  absl::flat_hash_map<size_t, KeyPtrSet> data_;  // KeyPtrSet now uses SharedString
};

} // namespace controller
