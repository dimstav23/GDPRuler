#pragma once

#include <iostream>
#include <optional>

#include <rocksdb/db.h>
#include <rocksdb/options.h>

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

    if (query.get_command() == "get" || query.get_command() == "getm" ) {
      return get(query.get_key());
    }
    if (query.get_command() == "put" || query.get_command() == "putm") {
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
};