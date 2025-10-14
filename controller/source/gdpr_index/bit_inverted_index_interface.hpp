#pragma once

#include "common_types.hpp"
#include <bitset>
#include <vector>
#include <cstddef>

namespace controller {

template<size_t NumBits>
class IBitInvertedIndex {
public:
  using BitmapType = std::bitset<NumBits>;
  
  virtual ~IBitInvertedIndex() = default;
  
  virtual void insert(const BitmapType& bits, const SharedString& key_ptr) = 0;
  virtual void remove(const SharedString& key_ptr) = 0;
  // find_any: uses union of bitmaps
  virtual std::vector<SharedString> find_any(const BitmapType& query_bits) const = 0;
  // find_all: uses intersection of bitmaps
  virtual std::vector<SharedString> find_all(const BitmapType& query_bits) const = 0;
  virtual void clear() = 0;
  virtual size_t size() const = 0;
  virtual const char* implementation_name() const = 0;
};

} // namespace controller
