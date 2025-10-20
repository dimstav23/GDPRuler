#include "index_manager.hpp"
#include "bit_inverted_index_hashset.hpp"
#include "bit_inverted_index_roaring.hpp"
#include "absl/container/flat_hash_set.h"
#include <algorithm>
#include <functional>

namespace controller {

IndexManager::IndexManager(const IndexConfig& config) : config_(config) {
  // Initialize shards
  key_shards_.reserve(config_.num_shards);
  for (size_t i = 0; i < config_.num_shards; ++i) {
    key_shards_.push_back(std::make_unique<KeyEntryShard>());
  }
  
  // Initialize owner index (single-bit hashmap)
  if (config_.enable_owner_index) {
    owner_index_ = std::make_unique<HashmapIndex>();
  }
  
  // Initialize origin index (single-bit hashmap)
  if (config_.enable_origin_index) {
    origin_index_ = std::make_unique<HashmapIndex>();
  }
  
  // Initialize expiration index (B+ tree)
  if (config_.enable_expiration_index) {
    expiration_index_ = std::make_unique<BTreeIndex>();
  }
  
  // Initialize purpose index (multi-bit, implementation selected at runtime)
  if (config_.enable_purpose_index) {
    // Preserved for the comparison benchmark
    if (config_.purpose_index_impl == BitInvertedIndexType::HASHSET) {
      purpose_index_ = std::make_unique<BitInvertedIndexHashSet<num_purposes>>();
    } 
    else {
      purpose_index_ = std::make_unique<BitInvertedIndexRoaring<num_purposes>>();
    }
  }
  
  // Initialize objection index
  if (config_.enable_objection_index) {
    objection_index_ = std::make_unique<BitInvertedIndexRoaring<num_purposes>>();
  }
  
  // Initialize share index
  if (config_.enable_share_index) {
    share_index_ = std::make_unique<BitInvertedIndexRoaring<num_users>>();
  }
}

IndexManager::~IndexManager() = default;

size_t IndexManager::get_shard_index(std::string_view key) const noexcept {
  return std::hash<std::string_view>{}(key) % config_.num_shards;
}

IndexManager::KeyEntryShard& IndexManager::get_shard(std::string_view key) noexcept {
  return *key_shards_[get_shard_index(key)];
}

const IndexManager::KeyEntryShard& IndexManager::get_shard(std::string_view key) const noexcept {
  return *key_shards_[get_shard_index(key)];
}

MetadataFingerprint IndexManager::extract_metadata(
  std::string_view complete_value,
  std::bitset<num_purposes>& purpose_bits,
  std::bitset<num_purposes>& objection_bits,
  std::bitset<num_users>& share_bits,
  size_t& owner_bit,
  size_t& origin_bit) const
{
  MetadataFingerprint fingerprint;
  if (complete_value.size() < sizeof(metadata_header)) {
    return fingerprint;
  }
  
  const auto* header = reinterpret_cast<const metadata_header*>(complete_value.data());
  fingerprint.expiration = header->expiration_time;
  
  size_t offset = sizeof(metadata_header);
  
  // Extract owner (single bit set)
  if (offset + header->user_bytes <= complete_value.size()) {
    const uint8_t* owner_bytes = reinterpret_cast<const uint8_t*>(complete_value.data() + offset);
    for (size_t i = 0; i < std::min<size_t>(num_users, header->user_bytes * 8); ++i) {
      if (owner_bytes[i / 8] & (1 << (i % 8))) {
        owner_bit = i;
        fingerprint.owner_bit = i;
        break;
      }
    }
    offset += header->user_bytes;
  }
  
  // Extract purpose
  if (offset + header->purpose_bytes <= complete_value.size()) {
    const uint8_t* purpose_bytes = reinterpret_cast<const uint8_t*>(complete_value.data() + offset);
    for (size_t i = 0; i < std::min<size_t>(num_purposes, header->purpose_bytes * 8); ++i) {
      if (purpose_bytes[i / 8] & (1 << (i % 8))) {
        purpose_bits.set(i);
      }
    }
    fingerprint.purpose_hash = std::hash<std::bitset<num_purposes>>{}(purpose_bits);
    offset += header->purpose_bytes;
  }
  
  // Extract objection
  if (offset + header->purpose_bytes <= complete_value.size()) {
    const uint8_t* objection_bytes = reinterpret_cast<const uint8_t*>(complete_value.data() + offset);
    for (size_t i = 0; i < std::min<size_t>(num_purposes, header->purpose_bytes * 8); ++i) {
      if (objection_bytes[i / 8] & (1 << (i % 8))) {
        objection_bits.set(i);
      }
    }
    fingerprint.objection_hash = std::hash<std::bitset<num_purposes>>{}(objection_bits);
    offset += header->purpose_bytes;
  }
  
  // Extract origin (single bit set)
  if (offset + header->origin_bytes <= complete_value.size()) {
    const uint8_t* origin_bytes = reinterpret_cast<const uint8_t*>(complete_value.data() + offset);
    for (size_t i = 0; i < std::min<size_t>(num_origins, header->origin_bytes * 8); ++i) {
      if (origin_bytes[i / 8] & (1 << (i % 8))) {
        origin_bit = i;
        fingerprint.origin_bit = i;
        break;
      }
    }
    offset += header->origin_bytes;
  }
  
  // Extract share
  if (offset + header->user_bytes <= complete_value.size()) {
    const uint8_t* share_bytes = reinterpret_cast<const uint8_t*>(complete_value.data() + offset);
    for (size_t i = 0; i < std::min<size_t>(num_users, header->user_bytes * 8); ++i) {
      if (share_bytes[i / 8] & (1 << (i % 8))) {
        share_bits.set(i);
      }
    }
    fingerprint.share_hash = std::hash<std::bitset<num_users>>{}(share_bits);
  }
  
  return fingerprint;
}

void IndexManager::on_put(std::string_view key, std::string_view complete_value) {
  std::bitset<num_purposes> purpose_bits, objection_bits;
  std::bitset<num_users> share_bits;
  size_t owner_bit = 0, origin_bit = 0;
  
  auto new_fingerprint = extract_metadata(complete_value, purpose_bits, objection_bits,
                                          share_bits, owner_bit, origin_bit);
  
  auto& shard = get_shard(key);
  std::unique_lock lock(shard.mutex);
  
  // Heterogeneous lookup: find by string_view
  auto it = shard.entries.find(key);
  
  if (it != shard.entries.end()) {
    // Key exists - get the SharedString from the map key
    const SharedString& key_ptr = it->first;  // ← Get key from map
    KeyEntry* entry = it->second.get();
    
    if (entry->fingerprint == new_fingerprint) {
        return;  // Metadata unchanged
    }
    
    // Metadata changed - need to update indexes
    remove_from_indexes(key_ptr, entry);  // Pass key_ptr
    
    entry->fingerprint = new_fingerprint;
    entry->purpose_bits = purpose_bits;
    entry->objection_bits = objection_bits;
    entry->share_bits = share_bits;
    
    insert_into_indexes(key_ptr, entry);  // Pass key_ptr
  } else {
    // New key
    auto entry = std::make_unique<KeyEntry>(new_fingerprint);
    entry->purpose_bits = purpose_bits;
    entry->objection_bits = objection_bits;
    entry->share_bits = share_bits;
    
    // Create shared_ptr ONCE
    SharedString key_ptr = std::make_shared<std::string>(std::string(key));
    KeyEntry* entry_raw = entry.get();
    
    // Store in shard (ref_count = 1)
    shard.entries.emplace(key_ptr, std::move(entry));
    
    // Pass to indexes (ref_count increases as indexes copy it)
    insert_into_indexes(key_ptr, entry_raw);
  }
}

void IndexManager::on_delete(std::string_view key) {
  auto& shard = get_shard(key);
  std::unique_lock lock(shard.mutex);
  
  auto it = shard.entries.find(key);  // Heterogeneous lookup
  if (it != shard.entries.end()) {
    // Get key_ptr from map before erasing
    const SharedString& key_ptr = it->first;
    KeyEntry* entry = it->second.get();
    
    // Remove from indexes BEFORE erasing from shard
    remove_from_indexes(key_ptr, entry);
    // Erase from shard (shared_ptr ref_count--, might free string)
    shard.entries.erase(it);
  }
}

void IndexManager::remove_from_indexes(const SharedString& key_ptr, KeyEntry* entry) {
  // const SharedString& key_ptr = entry->key;  // Reference to shared_ptr
  if (owner_index_) {
    owner_index_->remove(entry->fingerprint.owner_bit, key_ptr);
  }
  
  if (origin_index_) {
    origin_index_->remove(entry->fingerprint.origin_bit, key_ptr);
  }
  
  if (expiration_index_ && entry->fingerprint.expiration > 0) {
    expiration_index_->remove(entry->fingerprint.expiration, key_ptr);
  }
  
  if (purpose_index_) {
    purpose_index_->remove(key_ptr);
  }
  
  if (objection_index_) {
    objection_index_->remove(key_ptr);
  }
  
  if (share_index_) {
    share_index_->remove(key_ptr);
  }
}

void IndexManager::insert_into_indexes(const SharedString& key_ptr, KeyEntry* entry) {
  // const SharedString& key_ptr = entry->key;  
  if (owner_index_) {
    owner_index_->insert(entry->fingerprint.owner_bit, key_ptr);
  }
  
  if (origin_index_) {
    origin_index_->insert(entry->fingerprint.origin_bit, key_ptr);
  }
  
  if (expiration_index_ && entry->fingerprint.expiration > 0) {
    expiration_index_->insert(entry->fingerprint.expiration, key_ptr);
  }
  
  if (purpose_index_) {
    purpose_index_->insert(entry->purpose_bits, key_ptr);
  }
  
  if (objection_index_) {
    objection_index_->insert(entry->objection_bits, key_ptr);
  }
  
  if (share_index_) {
    share_index_->insert(entry->share_bits, key_ptr);
  }
}

std::vector<SharedString> IndexManager::find_by_owner(size_t owner_bit) const {
  if (!owner_index_) return {};
  return owner_index_->find(owner_bit);  // Returns vector<SharedString>
}

std::vector<SharedString> IndexManager::find_by_purpose(const std::bitset<num_purposes>& purpose_bits) const {
  if (!purpose_index_) return {};
  return purpose_index_->find_all(purpose_bits);
}

std::vector<SharedString> IndexManager::find_by_expiration_range(uint64_t start, uint64_t end) const {
  if (!expiration_index_) return {};
  return expiration_index_->find_range(start, end);
}

std::vector<SharedString> IndexManager::find_by_objection(const std::bitset<num_purposes>& objection_bits) const {
  if (!objection_index_) return {};
  return objection_index_->find_all(objection_bits);
}

std::vector<SharedString> IndexManager::find_by_origin(size_t origin_bit) const {
  if (!origin_index_) return {};
  return origin_index_->find(origin_bit);
}

std::vector<SharedString> IndexManager::find_by_share(const std::bitset<num_users>& share_bits) const {
  if (!share_index_) return {};
  return share_index_->find_any(share_bits);
}

std::vector<SharedString> IndexManager::find_keys(
  std::optional<size_t> owner_bit,
  std::optional<std::bitset<num_purposes>> purpose_bits,
  std::optional<uint64_t> expiration_threshold) const
{
  std::vector<SharedString> result;
  bool first_query = true;
  
  if (owner_bit.has_value()) {
    result = find_by_owner(*owner_bit);  // Returns SharedString
    first_query = false;
    if (result.empty()) return {};
  }
  
  if (purpose_bits.has_value()) {
    auto purpose_keys = find_by_purpose(*purpose_bits);
    
    if (first_query) {
      result = std::move(purpose_keys);
      first_query = false;
    } else {
      result = intersect_sets(result, purpose_keys);
      if (result.empty()) return {};
    }
  }
  
  if (expiration_threshold.has_value()) {
    auto exp_keys = find_by_expiration_range(0, *expiration_threshold);
    
    if (first_query) {
      result = std::move(exp_keys);
    } else {
      result = intersect_sets(result, exp_keys);
    }
  }
  
  return result;  // Returns SharedString - NO copies!
}

// intersect_sets now works with SharedString directly
std::vector<SharedString> IndexManager::intersect_sets(
  const std::vector<SharedString>& set1,
  const std::vector<SharedString>& set2)
{
  if (set1.empty() || set2.empty()) return {};
  
  const auto& smaller_set = (set1.size() <= set2.size()) ? set1 : set2;
  const auto& larger_set = (set1.size() <= set2.size()) ? set2 : set1;
  
  // Build hash set of SharedString
  absl::flat_hash_set<SharedString, SharedStringHash, SharedStringEqual> hash_set;
  hash_set.reserve(smaller_set.size());
  
  for (const auto& key_ptr : smaller_set) {
    hash_set.insert(key_ptr);  // Copies shared_ptr
  }
  
  std::vector<SharedString> result;
  result.reserve(std::min(set1.size(), set2.size()));
  
  for (const auto& key_ptr : larger_set) {
    if (hash_set.contains(key_ptr)) {
      result.push_back(key_ptr);  // Copies shared_ptr
    }
  }
  
  return result;
}

void IndexManager::on_bulk_put(const std::vector<std::pair<std::string_view, std::string_view>>& kv_pairs) {
  for (const auto& [key, value] : kv_pairs) {
    on_put(key, value);
  }
}

void IndexManager::on_bulk_delete(const std::vector<std::string_view>& keys) {
  for (const auto& key : keys) {
    on_delete(key);
  }
}

void IndexManager::clear() {
  if (owner_index_) owner_index_->clear();
  if (origin_index_) origin_index_->clear();
  if (expiration_index_) expiration_index_->clear();
  if (purpose_index_) purpose_index_->clear();
  if (objection_index_) objection_index_->clear();
  if (share_index_) share_index_->clear();

  for (auto& shard : key_shards_) {
    std::unique_lock lock(shard->mutex);
    shard->entries.clear();
  }
}

size_t IndexManager::total_indexed_keys() const {
  size_t total = 0;
  for (const auto& shard : key_shards_) {
    std::shared_lock lock(shard->mutex);
    total += shard->entries.size();
  }
  return total;
}

const char* IndexManager::get_purpose_impl_name() const {
  if (purpose_index_) {
    return purpose_index_->implementation_name();
  }
  return "None";
}

} // namespace controller
