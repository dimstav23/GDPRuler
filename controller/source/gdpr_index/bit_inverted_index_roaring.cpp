#include "bit_inverted_index_roaring.hpp"

namespace controller {

template<size_t NumBits>
void BitInvertedIndexRoaring<NumBits>::insert(const BitmapType& bits, const SharedString& key_ptr) {
  std::unique_lock lock(mutex_);
  
  uint32_t key_id;
  
  auto it = key_to_id_.find(key_ptr);
  if (it == key_to_id_.end()) {
    // Assign new ID
    if (!free_ids_.empty()) {
      key_id = free_ids_.back();
      free_ids_.pop_back();
      id_to_key_[key_id] = key_ptr;  // Store shared_ptr
    } else {
      key_id = next_key_id_++;
      id_to_key_.push_back(key_ptr);  // Store shared_ptr
    }
    key_to_id_[key_ptr] = key_id;
  } else {
    key_id = it->second;
  }
  
  // Add to Roaring bitmaps
  for (size_t i = 0; i < NumBits; ++i) {
    if (bits.test(i)) {
      bit_index_[i].add(key_id);
    }
  }
  
  key_to_bits_[key_ptr] = bits;
}

template<size_t NumBits>
void BitInvertedIndexRoaring<NumBits>::remove(const SharedString& key_ptr) {
  std::unique_lock lock(mutex_);
  
  auto id_it = key_to_id_.find(key_ptr);
  if (id_it == key_to_id_.end()) return;
  
  uint32_t key_id = id_it->second;
  
  // Remove from bit indexes
  auto bits_it = key_to_bits_.find(key_ptr);
  if (bits_it != key_to_bits_.end()) {
    const auto& bits = bits_it->second;
    for (size_t i = 0; i < NumBits; ++i) {
      if (bits.test(i)) {
        bit_index_[i].remove(key_id);
      }
    }
    key_to_bits_.erase(bits_it);
  }
  
  // Mark ID as free
  id_to_key_[key_id] = nullptr;  // Release shared_ptr
  key_to_id_.erase(id_it);
  free_ids_.push_back(key_id);
}

// find_any: uses union of Roaring bitmaps
template<size_t NumBits>
std::vector<SharedString> BitInvertedIndexRoaring<NumBits>::find_any(const BitmapType& query_bits) const {
  std::shared_lock lock(mutex_);
  
  size_t num_query_bits = query_bits.count();
  if (num_query_bits == 0) return {};
  
  if (num_query_bits == 1) {
    for (size_t i = 0; i < NumBits; ++i) {
      if (query_bits.test(i)) {
        return roaring_to_keys(bit_index_[i]);
      }
    }
  }
  
  // Multi-bit: Fast Roaring union
  roaring::Roaring result;
  for (size_t i = 0; i < NumBits; ++i) {
    if (query_bits.test(i)) {
      result |= bit_index_[i];
    }
  }
  
  return roaring_to_keys(result);
}

// find_all: uses intersection of Roaring bitmaps
template<size_t NumBits>
std::vector<SharedString> BitInvertedIndexRoaring<NumBits>::find_all(const BitmapType& query_bits) const {
  std::shared_lock lock(mutex_);
  
  size_t first_bit = NumBits;
  for (size_t i = 0; i < NumBits; ++i) {
    if (query_bits.test(i)) {
      first_bit = i;
      break;
    }
  }
  
  if (first_bit == NumBits) return {};
  
  roaring::Roaring result = bit_index_[first_bit];
  
  for (size_t i = first_bit + 1; i < NumBits; ++i) {
    if (query_bits.test(i)) {
      result &= bit_index_[i];
      if (result.isEmpty()) return {};
    }
  }
  
  return roaring_to_keys(result);
}

template<size_t NumBits>
std::vector<SharedString> BitInvertedIndexRoaring<NumBits>::roaring_to_keys(const roaring::Roaring& bitmap) const {
  std::vector<SharedString> result;
  result.reserve(bitmap.cardinality());
  
  for (uint32_t key_id : bitmap) {
    if (key_id < id_to_key_.size()) {
      const SharedString& key_ptr = id_to_key_[key_id];
      if (key_ptr != nullptr) {
        result.push_back(key_ptr);  // Copies shared_ptr, ref_count++
      }
    }
  }
  
  return result;
}

template<size_t NumBits>
void BitInvertedIndexRoaring<NumBits>::clear() {
  std::unique_lock lock(mutex_);
  for (auto& bitmap : bit_index_) {
    bitmap = roaring::Roaring();
  }
  key_to_id_.clear();
  id_to_key_.clear();
  key_to_bits_.clear();
  free_ids_.clear();
  next_key_id_ = 0;
}

template<size_t NumBits>
size_t BitInvertedIndexRoaring<NumBits>::size() const {
  std::shared_lock lock(mutex_);
  return key_to_bits_.size();
}

// Explicit template instantiation
template class BitInvertedIndexRoaring<128>;

} // namespace controller
