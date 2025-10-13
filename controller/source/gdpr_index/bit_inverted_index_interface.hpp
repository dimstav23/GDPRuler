#pragma once

#include <bitset>
#include <vector>
#include <string>
#include <cstddef>

namespace controller {

/**
 * Abstract interface for BitInvertedIndex implementations
 * Allows runtime selection between HashSet and Roaring implementations
 */
template<size_t NumBits>
class IBitInvertedIndex {
public:
  using BitmapType = std::bitset<NumBits>;
  
  virtual ~IBitInvertedIndex() = default;
  
  virtual void insert(const BitmapType& bits, const std::string* key_ptr) = 0;
  virtual void remove(const std::string* key_ptr) = 0;
  virtual std::vector<const std::string*> find_any(const BitmapType& query_bits) const = 0;
  virtual std::vector<const std::string*> find_all(const BitmapType& query_bits) const = 0;
  virtual void clear() = 0;
  virtual size_t size() const = 0;
  virtual const char* implementation_name() const = 0;
};

} // namespace controller
