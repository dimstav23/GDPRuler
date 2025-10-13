#pragma once

#include "common_types.hpp"
#include "absl/container/btree_map.h"
#include <vector>
#include <shared_mutex>
#include <cstdint>

namespace controller {

/**
 * BTreeIndex: Range queries for timestamps (expiration)
 * 
 * Uses Abseil's B+ tree for better range scan performance
 * compared to std::map due to better cache locality
 */
class BTreeIndex {
public:
  BTreeIndex() = default;
  
  void insert(uint64_t timestamp, const std::string* key_ptr);
  void remove(uint64_t timestamp, const std::string* key_ptr);
  std::vector<const std::string*> find_range(uint64_t start, uint64_t end) const;
  void clear();
  size_t size() const;

private:
  mutable std::shared_mutex mutex_;
  absl::btree_multimap<uint64_t, const std::string*> data_;
};

} // namespace controller
