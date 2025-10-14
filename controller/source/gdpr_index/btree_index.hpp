#pragma once

#include "common_types.hpp"
#include "absl/container/btree_map.h"
#include <vector>
#include <cstdint>

namespace controller {

class BTreeIndex {
public:
  BTreeIndex() = default;
  
  void insert(uint64_t timestamp, const SharedString& key_ptr);
  void remove(uint64_t timestamp, const SharedString& key_ptr);
  std::vector<SharedString> find_range(uint64_t start, uint64_t end) const;
  void clear();
  size_t size() const;

private:
  mutable std::shared_mutex mutex_;
  absl::btree_multimap<uint64_t, SharedString> data_;  // Now stores shared_ptr
};

} // namespace controller
