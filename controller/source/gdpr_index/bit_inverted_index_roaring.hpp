#pragma once

#include "bit_inverted_index_interface.hpp"
#include "common_types.hpp"
#include "absl/container/flat_hash_map.h"
#include "roaring/roaring.hh"
#include <shared_mutex>
#include <array>
#include <vector>
#include <cstdint>

namespace controller {

/**
 * BitInvertedIndexRoaring: Roaring bitmap implementation
 * 
 * Excellent for multi-bit queries (30-50× faster than HashSet)
 * Uses compressed bitmaps for memory efficiency
 */
template<size_t NumBits>
class BitInvertedIndexRoaring : public IBitInvertedIndex<NumBits> {
public:
  using BitmapType = typename IBitInvertedIndex<NumBits>::BitmapType;
  
  BitInvertedIndexRoaring() : next_key_id_(0) {}
  
  void insert(const BitmapType& bits, const std::string* key_ptr) override;
  void remove(const std::string* key_ptr) override;
  std::vector<const std::string*> find_any(const BitmapType& query_bits) const override;
  std::vector<const std::string*> find_all(const BitmapType& query_bits) const override;
  void clear() override;
  size_t size() const override;
  const char* implementation_name() const override { return "Roaring"; }

private:
  mutable std::shared_mutex mutex_;
  
  // Key ID management
  uint32_t next_key_id_;
  absl::flat_hash_map<const std::string*, uint32_t, StringPtrHash, StringPtrEqual> key_to_id_;
  std::vector<const std::string*> id_to_key_;
  std::vector<uint32_t> free_ids_;
  
  // Roaring bitmaps per bit position
  std::array<roaring::Roaring, NumBits> bit_index_;
  
  // Reverse mapping for efficient deletion
  absl::flat_hash_map<const std::string*, BitmapType, StringPtrHash, StringPtrEqual> key_to_bits_;
  
  std::vector<const std::string*> roaring_to_keys(const roaring::Roaring& bitmap) const;
};

} // namespace controller
