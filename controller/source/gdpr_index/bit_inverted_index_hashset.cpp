#include "bit_inverted_index_hashset.hpp"

namespace controller {

template<size_t NumBits>
void BitInvertedIndexHashSet<NumBits>::insert(const BitmapType& bits, const SharedString& key_ptr) {
  std::unique_lock lock(mutex_);
  
  for (size_t i = 0; i < NumBits; ++i) {
    if (bits.test(i)) {
      bit_to_keys_[i].insert(key_ptr);  // Copies shared_ptr
    }
  }
  
  key_to_bits_[key_ptr] = bits;
}

template<size_t NumBits>
void BitInvertedIndexHashSet<NumBits>::remove(const SharedString& key_ptr) {
  std::unique_lock lock(mutex_);
  
  auto it = key_to_bits_.find(key_ptr);
  if (it == key_to_bits_.end()) return;
  
  const BitmapType& bits = it->second;
  for (size_t i = 0; i < NumBits; ++i) {
    if (bits.test(i)) {
      auto bit_it = bit_to_keys_.find(i);
      if (bit_it != bit_to_keys_.end()) {
        bit_it->second.erase(key_ptr);  // Releases shared_ptr
        if (bit_it->second.empty()) {
          bit_to_keys_.erase(bit_it);
        }
      }
    }
  }
  
  key_to_bits_.erase(it);
}

template<size_t NumBits>
std::vector<SharedString> BitInvertedIndexHashSet<NumBits>::find_any(const BitmapType& query_bits) const {
  std::shared_lock lock(mutex_);
  
  size_t num_query_bits = query_bits.count();
  if (num_query_bits == 0) return {};
  
  if (num_query_bits == 1) {
    for (size_t i = 0; i < NumBits; ++i) {
      if (query_bits.test(i)) {
        auto it = bit_to_keys_.find(i);
        if (it != bit_to_keys_.end()) {
          return std::vector<SharedString>(it->second.begin(), it->second.end());
        }
        return {};
      }
    }
  }
  
  // Multi-bit: Build hash set
  KeyPtrSet result_set;
  for (size_t i = 0; i < NumBits; ++i) {
    if (query_bits.test(i)) {
      auto it = bit_to_keys_.find(i);
      if (it != bit_to_keys_.end()) {  // ← Compare with map's end()
        return std::vector<SharedString>(it->second.begin(), it->second.end());
      }
    }
  }
  
  return std::vector<SharedString>(result_set.begin(), result_set.end());
}

template<size_t NumBits>
std::vector<SharedString> BitInvertedIndexHashSet<NumBits>::find_all(const BitmapType& query_bits) const {
  std::shared_lock lock(mutex_);
  
  size_t first_bit = NumBits;
  for (size_t i = 0; i < NumBits; ++i) {
    if (query_bits.test(i)) {
      first_bit = i;
      break;
    }
  }
  
  if (first_bit == NumBits) return {};
  
  auto it = bit_to_keys_.find(first_bit);
  if (it == bit_to_keys_.end()) return {};
  
  KeyPtrSet result(it->second.begin(), it->second.end());
  
  for (size_t i = first_bit + 1; i < NumBits; ++i) {
    if (query_bits.test(i)) {
      auto bit_it = bit_to_keys_.find(i);
      if (bit_it == bit_to_keys_.end()) {
        return {};
      }
      
      KeyPtrSet intersection;
      for (const auto& key_ptr : result) {
        if (bit_it->second.find(key_ptr) != bit_it->second.end()) {
          intersection.insert(key_ptr);
        }
      }
      result = std::move(intersection);
      
      if (result.empty()) return {};
    }
  }
  
  return std::vector<SharedString>(result.begin(), result.end());
}

template<size_t NumBits>
void BitInvertedIndexHashSet<NumBits>::clear() {
  std::unique_lock lock(mutex_);
  bit_to_keys_.clear();
  key_to_bits_.clear();
}

template<size_t NumBits>
size_t BitInvertedIndexHashSet<NumBits>::size() const {
  std::shared_lock lock(mutex_);
  return key_to_bits_.size();
}

// Explicit template instantiation
template class BitInvertedIndexHashSet<128>;

} // namespace controller
