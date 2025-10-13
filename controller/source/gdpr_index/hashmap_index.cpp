#include "hashmap_index.hpp"

namespace controller {

void HashmapIndex::insert(size_t bit_position, const std::string* key_ptr) {
  std::unique_lock lock(mutex_);
  data_[bit_position].insert(key_ptr);
}

void HashmapIndex::remove(size_t bit_position, const std::string* key_ptr) {
  std::unique_lock lock(mutex_);
  auto it = data_.find(bit_position);
  if (it != data_.end()) {
    it->second.erase(key_ptr);
    if (it->second.empty()) {
      data_.erase(it);
    }
  }
}

std::vector<const std::string*> HashmapIndex::find(size_t bit_position) const {
  std::shared_lock lock(mutex_);
  auto it = data_.find(bit_position);
  if (it != data_.end()) {
    return std::vector<const std::string*>(it->second.begin(), it->second.end());
  }
  return {};
}

void HashmapIndex::clear() {
  std::unique_lock lock(mutex_);
  data_.clear();
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
