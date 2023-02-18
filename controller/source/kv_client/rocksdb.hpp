#pragma once

#include <rocksdb/db.h>
#include <rocksdb/options.h>

#include "kv_client.hpp"

class rocksdb_client : public kv_client
{
rocksdb::DB* m_rocksdb{nullptr};

public:
  explicit rocksdb_client(const std::string& db_path) {
    rocksdb::Options options;
    options.create_if_missing = true;
    rocksdb::Status status = rocksdb::DB::Open(options, db_path, &m_rocksdb);
    if (!status.ok()) {
      std::cerr << "Failed to open database: " << status.ToString() << std::endl;
    }
  }

  auto get(const std::string& key) -> std::optional<std::string> override
  {
    std::string value;
    rocksdb::Status status = m_rocksdb->Get(rocksdb::ReadOptions(), key, &value);
    if (status.ok()) {
      std::cout << "GET operation succeeded! Key: " << key << ", Value: " << value << std::endl;
    } else {
      std::cout << "GET operation failed" << std::endl;
    }
    return value;
  }

  auto put(const std::string& key, const std::string& value) -> bool override
  {
    rocksdb::Status status = m_rocksdb->Put(rocksdb::WriteOptions(), key, value);
    if (status.ok()) {
      std::cout << "PUT operation succeeded! Key: " << key << ", Value: " << value << std::endl;
    } else {
      std::cout << "PUT operation failed" << std::endl;
    }
    return status.ok();
  }

  auto del(const std::string& key) -> bool override
  {
    rocksdb::Status status = m_rocksdb->Delete(rocksdb::WriteOptions(), key);
    if (status.ok()) {
      std::cout << "DELETE operation succeeded! Key: " << key << std::endl;
    } else {
      std::cout << "DELETE operation failed" << std::endl;
    }
    return status.ok();
  }

  ~rocksdb_client() override {
    delete m_rocksdb;
  }

  rocksdb_client() = default;
  rocksdb_client(const rocksdb_client&) = default;
  auto operator=(rocksdb_client const&) -> rocksdb_client& = default;
  rocksdb_client(rocksdb_client&&) = default;
  auto operator=(rocksdb_client&&) -> rocksdb_client& = default;
};