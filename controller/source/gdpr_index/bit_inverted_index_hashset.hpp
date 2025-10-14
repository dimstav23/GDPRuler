#pragma once

#include "bit_inverted_index_interface.hpp"
#include "common_types.hpp"
#include "absl/container/flat_hash_map.h"
#include <shared_mutex>

namespace controller {

template<size_t NumBits>
class BitInvertedIndexHashSet : public IBitInvertedIndex<NumBits> {
public:
  using BitmapType = typename IBitInvertedIndex<NumBits>::BitmapType;
  
  BitInvertedIndexHashSet() = default;
  
  void insert(const BitmapType& bits, const SharedString& key_ptr) override;
  void remove(const SharedString& key_ptr) override;
  std::vector<SharedString> find_any(const BitmapType& query_bits) const override;
  std::vector<SharedString> find_all(const BitmapType& query_bits) const override;
  void clear() override;
  size_t size() const override;
  const char* implementation_name() const override { return "HashSet"; }

private:
  mutable std::shared_mutex mutex_;
  absl::flat_hash_map<size_t, KeyPtrSet> bit_to_keys_;  // KeyPtrSet uses SharedString
  absl::flat_hash_map<SharedString, BitmapType, SharedStringHash, SharedStringEqual> key_to_bits_;
};

} // namespace controller
