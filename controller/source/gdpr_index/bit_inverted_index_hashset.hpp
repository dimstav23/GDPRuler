#pragma once

#include "bit_inverted_index_interface.hpp"
#include "common_types.hpp"
#include "absl/container/flat_hash_map.h"
#include <shared_mutex>

namespace controller {

/**
 * BitInvertedIndexHashSet: Hash-based implementation
 * 
 * Good for single-bit queries (10k ops/sec)
 * Slower for multi-bit queries due to hash set deduplication overhead
 */
template<size_t NumBits>
class BitInvertedIndexHashSet : public IBitInvertedIndex<NumBits> {
public:
  using BitmapType = typename IBitInvertedIndex<NumBits>::BitmapType;
  
  BitInvertedIndexHashSet() = default;
  
  void insert(const BitmapType& bits, const std::string* key_ptr) override;
  void remove(const std::string* key_ptr) override;
  std::vector<const std::string*> find_any(const BitmapType& query_bits) const override;
  std::vector<const std::string*> find_all(const BitmapType& query_bits) const override;
  void clear() override;
  size_t size() const override;
  const char* implementation_name() const override { return "HashSet"; }

private:
  mutable std::shared_mutex mutex_;
  absl::flat_hash_map<size_t, KeyPtrSet> bit_to_keys_;
  absl::flat_hash_map<const std::string*, BitmapType, StringPtrHash, StringPtrEqual> key_to_bits_;
};

} // namespace controller
