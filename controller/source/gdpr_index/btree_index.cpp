#include "btree_index.hpp"
#include <algorithm>

namespace controller {

void BTreeIndex::insert(uint64_t timestamp, const SharedString& key_ptr) {
  std::unique_lock lock(mutex_);
  data_.insert({timestamp, key_ptr});  // Copies shared_ptr
}

void BTreeIndex::remove(uint64_t timestamp, const SharedString& key_ptr) {
  std::unique_lock lock(mutex_);
  auto range = data_.equal_range(timestamp);
  
  std::vector<decltype(data_)::iterator> to_erase;
  for (auto it = range.first; it != range.second; ++it) {
    if (SharedStringEqual{}(it->second, key_ptr)) {  // Compare string contents
      to_erase.push_back(it);
    }
  }
  
  for (auto it : to_erase) {
    data_.erase(it);  // shared_ptr released, ref_count--
  }
}

std::vector<SharedString> BTreeIndex::find_range(uint64_t start, uint64_t end) const {
  std::shared_lock lock(mutex_);
  
  std::vector<SharedString> result;
  auto it_start = data_.lower_bound(start);
  auto it_end = data_.upper_bound(end);
  
  result.reserve(std::distance(it_start, it_end));
  
  for (auto it = it_start; it != it_end; ++it) {
    result.push_back(it->second);  // Copies shared_ptr, ref_count++
  }
  
  return result;
}

void BTreeIndex::clear() {
  std::unique_lock lock(mutex_);
  data_.clear();
}

size_t BTreeIndex::size() const {
  std::shared_lock lock(mutex_);
  return data_.size();
}

} // namespace controller
