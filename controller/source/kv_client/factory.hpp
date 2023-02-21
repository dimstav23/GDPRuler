#pragma once

#include <string>

#include "kv_client.hpp"
#include "redis.hpp"
#include "rocksdb.hpp"


class kv_factory {
  
  /* Get default address for given kv_backend. */
  static auto get_default_address(const std::string& kv_backend) -> std::string {
    if (kv_backend == "redis") {
      return "tcp://127.0.0.1:6379";
    } 
    if (kv_backend == "rocksdb") {
      return "./db";
    }
    throw std::runtime_error("Unsupported KV backend");
  }

public:

  kv_factory() = delete;

  /* Create kv_client object with given kv_backend and address. */
  static auto create(const std::string& kv_backend, std::string address) -> std::unique_ptr<kv_client> {
    if (address.empty()) {
      address = get_default_address(kv_backend);
    }

    std::cout << "Creating kv_client. Type: " << kv_backend << ", address: " << address << std::endl;

    if (kv_backend == "redis") {
      return std::make_unique<redis_client>(address);
    } 
    if (kv_backend == "rocksdb") {
      return std::make_unique<rocksdb_client>(address);
    }
    std::cerr << "Unsupported KV backend: " << kv_backend << std::endl;
    std::quick_exit(1);
  }
};
