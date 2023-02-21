#pragma once

#include <iostream>
#include <string>
#include <optional>

class kv_client
{
public:
  /* kv_client interface signatures */
  virtual auto get(const std::string& key) -> std::optional<std::string> = 0;
  virtual auto put(const std::string& key, const std::string& value) -> bool = 0;
  virtual auto del(const std::string& key) -> bool = 0;

  /* Constructors, destructors, etc */
  virtual ~kv_client() = default;
  kv_client() = default;
  kv_client(const kv_client&) = default;
  auto operator=(kv_client const&) -> kv_client& = default;
  kv_client(kv_client&&) = default;
  auto operator=(kv_client&&) -> kv_client& = default;
};