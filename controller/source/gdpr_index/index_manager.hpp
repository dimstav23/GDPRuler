#pragma once

#include "hashmap_index.hpp"
#include "btree_index.hpp"
#include "bit_inverted_index_interface.hpp"
#include "index_config.hpp"
#include "common_types.hpp"
#include "gdpr_metadata.hpp"
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <bitset>
#include <unordered_map>

namespace controller {

class IndexManager {
public:
  explicit IndexManager(const IndexConfig& config = IndexConfig{});
  ~IndexManager();
  
  IndexManager(const IndexManager&) = delete;
  IndexManager& operator=(const IndexManager&) = delete;
  
  // Index maintenance
  void on_put(std::string_view key, std::string_view complete_value);
  void on_delete(std::string_view key);
  void on_bulk_put(const std::vector<std::pair<std::string_view, std::string_view>>& kv_pairs);
  void on_bulk_delete(const std::vector<std::string_view>& keys);
  
  // Query operations
  std::vector<SharedString> find_by_owner(size_t owner_bit) const;
  std::vector<SharedString> find_by_purpose(const std::bitset<num_purposes>& purpose_bits) const;
  std::vector<SharedString> find_by_expiration_range(uint64_t start, uint64_t end) const;
  std::vector<SharedString> find_by_objection(const std::bitset<num_purposes>& objection_bits) const;
  std::vector<SharedString> find_by_origin(size_t origin_bit) const;
  std::vector<SharedString> find_by_share(const std::bitset<num_users>& share_bits) const;
  
  std::vector<SharedString> find_keys(
    std::optional<size_t> owner_bit = std::nullopt,
    std::optional<std::bitset<num_purposes>> purpose_bits = std::nullopt,
    std::optional<uint64_t> expiration_threshold = std::nullopt
  ) const;
  
  // Management
  void clear();
  size_t total_indexed_keys() const;
  const char* get_purpose_impl_name() const;

private:
  IndexConfig config_;
  
  struct KeyEntry {
    // SharedString key;
    MetadataFingerprint fingerprint;
    std::bitset<num_purposes> purpose_bits;
    std::bitset<num_purposes> objection_bits;
    std::bitset<num_users> share_bits;
    
    // KeyEntry(std::string k, MetadataFingerprint fp)
    //   : key(std::make_shared<std::string>(std::move(k))), 
    //     fingerprint(std::move(fp)) {}
    KeyEntry(MetadataFingerprint fp)
      : fingerprint(std::move(fp)) {}
  };
  
  struct KeyEntryShard {
    mutable std::shared_mutex mutex;
    // Use SharedString as key with hash/equal from common_types.hpp
    // use the SharedString here to keep it alive
    std::unordered_map<SharedString, std::unique_ptr<KeyEntry>, 
              SharedStringHash, SharedStringEqual> entries;
  };
  
  std::vector<std::unique_ptr<KeyEntryShard>> key_shards_;
  
  // Indexes
  std::unique_ptr<HashmapIndex> owner_index_;
  std::unique_ptr<HashmapIndex> origin_index_;
  std::unique_ptr<BTreeIndex> expiration_index_;
  std::unique_ptr<IBitInvertedIndex<num_purposes>> purpose_index_;
  std::unique_ptr<IBitInvertedIndex<num_purposes>> objection_index_;
  std::unique_ptr<IBitInvertedIndex<num_users>> share_index_;
  
  // Helper methods
  size_t get_shard_index(std::string_view key) const noexcept;
  KeyEntryShard& get_shard(std::string_view key) noexcept;
  const KeyEntryShard& get_shard(std::string_view key) const noexcept;
  
  MetadataFingerprint extract_metadata(
    std::string_view complete_value,
    std::bitset<num_purposes>& purpose_bits,
    std::bitset<num_purposes>& objection_bits,
    std::bitset<num_users>& share_bits,
    size_t& owner_bit,
    size_t& origin_bit) const;
  
  void remove_from_indexes(const SharedString& key_ptr, KeyEntry* entry);
  void insert_into_indexes(const SharedString& key_ptr, KeyEntry* entry);
  
  static std::vector<SharedString> intersect_sets(
    const std::vector<SharedString>& set1,
    const std::vector<SharedString>& set2);
};

} // namespace controller
