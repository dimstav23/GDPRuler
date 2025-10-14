#pragma once

#include "bit_inverted_index_interface.hpp"
#include "common_types.hpp"
#include "absl/container/flat_hash_map.h"
#include "roaring/roaring.hh"
#include <array>
#include <vector>
#include <cstdint>

namespace controller {

template<size_t NumBits>
class BitInvertedIndexRoaring : public IBitInvertedIndex<NumBits> {
public:
  using BitmapType = typename IBitInvertedIndex<NumBits>::BitmapType;
  
  BitInvertedIndexRoaring() : next_key_id_(0) {}
  
  void insert(const BitmapType& bits, const SharedString& key_ptr) override;
  void remove(const SharedString& key_ptr) override;
  // find_any: uses union of Roaring bitmaps
  std::vector<SharedString> find_any(const BitmapType& query_bits) const override;
  // find_all: uses intersection of Roaring bitmaps
  std::vector<SharedString> find_all(const BitmapType& query_bits) const override;
  void clear() override;
  size_t size() const override;
  const char* implementation_name() const override { return "Roaring"; }

private:
  mutable std::shared_mutex mutex_;
  
  // Key ID management
  uint32_t next_key_id_;
  absl::flat_hash_map<SharedString, uint32_t, SharedStringHash, SharedStringEqual> key_to_id_;
  std::vector<SharedString> id_to_key_;  // Now stores shared_ptr!
  std::vector<uint32_t> free_ids_;
  
  // Roaring bitmaps per bit position
  std::array<roaring::Roaring, NumBits> bit_index_;
  
  // Reverse mapping for efficient deletion
  absl::flat_hash_map<SharedString, BitmapType, SharedStringHash, SharedStringEqual> key_to_bits_;
  
  std::vector<SharedString> roaring_to_keys(const roaring::Roaring& bitmap) const;
};

} // namespace controller