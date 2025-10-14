#include "hashmap_index.hpp"

namespace controller {

void HashmapIndex::insert(size_t bit_position, const SharedString& key_ptr) {
  std::unique_lock lock(mutex_);
  data_[bit_position].insert(key_ptr);  // Copies shared_ptr, ref_count++
}

void HashmapIndex::remove(size_t bit_position, const SharedString& key_ptr) {
  std::unique_lock lock(mutex_);
  auto it = data_.find(bit_position);
  if (it != data_.end()) {
    it->second.erase(key_ptr);  // Removes shared_ptr, ref_count--
    if (it->second.empty()) {
      data_.erase(it);
    }
  }
}

// Returns shared_ptr
std::vector<SharedString> HashmapIndex::find(size_t bit_position) const {
  std::shared_lock lock(mutex_);
  auto it = data_.find(bit_position);
  if (it != data_.end()) {
    // Copy all shared_ptr from set to vector
    // Each copy increments ref_count atomically
    return std::vector<SharedString>(it->second.begin(), it->second.end());
  }
  return {};
}

void HashmapIndex::clear() {
  std::unique_lock lock(mutex_);
  data_.clear();  // All shared_ptr released, ref_counts decremented
}

size_t HashmapIndex::size() const {
  std::shared_lock lock(mutex_);
  size_t total = 0;
  for (const auto& [bit_pos, keys] : data_) {
    total += keys.size();
  }
  return total;
}

} // namespace controller
