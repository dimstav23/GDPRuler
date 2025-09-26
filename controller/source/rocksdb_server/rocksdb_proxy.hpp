#pragma once

#include <iostream>
#include <optional>

#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include "rocksdb/slice_transform.h"

#include "message.hpp"

/**
 * rocksdb_proxy is a wrapper to interact with the rocksdb.
*/
class rocksdb_proxy
{
public:
  explicit rocksdb_proxy(const std::string& db_path)
  {
    rocksdb::Options options;
    options.create_if_missing = true;

    // Enable prefix bloom filter for better scan performance
    int default_prefix_length = 3;
    options.prefix_extractor.reset(rocksdb::NewFixedPrefixTransform(default_prefix_length));

    rocksdb::Status status = rocksdb::DB::Open(options, db_path, &m_rocksdb);
    if (!status.ok()) {
      std::cerr << "Failed to open database: " << status.ToString()
                << std::endl;
    }
  }

  auto execute(query_message query) -> response_message 
  {
    if (!query.get_is_valid()) {
      return response_message{/*is_success*/false, ""};
    }

    if (query.get_command() == "get" ) {
      return get(query.get_key());
    }
    if (query.get_command() == "getm" ) {
      return getm(query.get_key());
    }
    if (query.get_command() == "put" || query.get_command() == "putm" || query.get_command() == "putc") {
      return put(query.get_key(), query.get_value());
    }
    return del(query.get_key());
  }

  ~rocksdb_proxy() { delete m_rocksdb; }

  rocksdb_proxy() = default;
  rocksdb_proxy(const rocksdb_proxy&) = default;
  auto operator=(rocksdb_proxy const&) -> rocksdb_proxy& = default;
  rocksdb_proxy(rocksdb_proxy&&) = default;
  auto operator=(rocksdb_proxy&&) -> rocksdb_proxy& = default;

private:
  rocksdb::DB* m_rocksdb {nullptr};

  auto get(std::string_view key) -> response_message
  {
    std::string value;
    rocksdb::Status status =
        m_rocksdb->Get(rocksdb::ReadOptions(), key, &value);
    if (status.ok()) {
      return response_message{/*is_success*/true, value};
    }
    return response_message{/*is_success*/false, ""};
  }

  auto put(std::string_view key, std::string_view value) -> response_message
  {
    rocksdb::Status status =
        m_rocksdb->Put(rocksdb::WriteOptions(), key, value);
    if (status.ok()) {
      return response_message{/*is_success*/true, ""};
    } 
    return response_message{/*is_success*/false, ""};
  }

  auto del(std::string_view key) -> response_message
  {
    rocksdb::Status status = m_rocksdb->Delete(rocksdb::WriteOptions(), key);
    if (status.ok()) {
      return response_message{/*is_success*/true, ""};
    } 
    return response_message{/*is_success*/false, ""};
  }

  /* response format: [4 bytes: size1][data1][4 bytes: size2][data2]...[4 bytes: sizeN][dataN] */
  auto getm(std::string_view key_prefix) -> response_message
  {
      rocksdb::ReadOptions read_options;
      
      std::string upper_bound = std::string(key_prefix) + char(255);
      rocksdb::Slice upper_bound_slice(upper_bound);
      read_options.iterate_upper_bound = &upper_bound_slice;
      read_options.prefix_same_as_start = true;
      
      std::unique_ptr<rocksdb::Iterator> it(m_rocksdb->NewIterator(read_options));
      it->Seek(key_prefix);
      
      if (!it->Valid()) {
        return response_message{/*is_success*/false, ""};
      }
      
      std::string result;
      result.reserve(4096); // Start with reasonable capacity
      
      while (it->Valid()) {
        rocksdb::Slice value_slice = it->value();
        uint32_t value_size = static_cast<uint32_t>(value_slice.size());
        
        // Calculate required space for this entry
        size_t entry_size = sizeof(uint32_t) + value_size;
        size_t current_pos = result.size();
        
        // Grow buffer if needed
        if (current_pos + entry_size > result.capacity()) {
          result.reserve(std::max(current_pos + entry_size, result.capacity() * 2));
        }
        
        // Resize string to accommodate new data
        result.resize(current_pos + entry_size);
        
        // Get direct pointer to write position
        char* write_ptr = result.data() + current_pos;
        
        // Write size directly
        std::memcpy(write_ptr, &value_size, sizeof(value_size));
        
        // Write value data directly - zero copy from RocksDB buffer
        std::memcpy(write_ptr + sizeof(uint32_t), value_slice.data(), value_size);
        
        it->Next();
      }
      
      return response_message{/*is_success*/true, std::move(result)};
  }
};