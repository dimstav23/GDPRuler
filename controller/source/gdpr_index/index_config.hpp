#pragma once

#include <cstddef>
#include <cstdint>

namespace controller {

// Implementation type for multi-bit inverted indexes
enum class BitInvertedIndexType {
  HASHSET,
  ROARING   // Roaring bitmaps (faster for multi-bit queries)
};

struct IndexConfig {
  // Which indexes to enable
  bool enable_owner_index = true;
  bool enable_purpose_index = true;
  bool enable_expiration_index = true;
  bool enable_objection_index = false;
  bool enable_origin_index = true;
  bool enable_share_index = false;
  
  // Implementation selection for multi-bit indexes
  BitInvertedIndexType purpose_index_impl = BitInvertedIndexType::ROARING;
  BitInvertedIndexType objection_index_impl = BitInvertedIndexType::ROARING;
  BitInvertedIndexType share_index_impl = BitInvertedIndexType::ROARING;
  
  // Sharding for fine-grained locking
  size_t num_shards = 64;
};

// Metadata fingerprint for change detection
struct MetadataFingerprint {
  size_t owner_bit = 0;
  uint64_t purpose_hash = 0;
  uint64_t expiration = 0;
  uint64_t objection_hash = 0;
  size_t origin_bit = 0;
  uint64_t share_hash = 0;
  
  bool operator==(const MetadataFingerprint& other) const noexcept {
    return owner_bit == other.owner_bit &&
         purpose_hash == other.purpose_hash &&
         expiration == other.expiration &&
         objection_hash == other.objection_hash &&
         origin_bit == other.origin_bit &&
         share_hash == other.share_hash;
  }
  
  bool operator!=(const MetadataFingerprint& other) const noexcept {
    return !(*this == other);
  }
};

} // namespace controller
