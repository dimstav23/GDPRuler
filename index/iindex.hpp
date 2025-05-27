#pragma once

#include <set>

#include "query_condition.hpp"

template<typename K, typename V>
class IIndex {
public:
  virtual auto insert(const K& key, const V& value) -> void = 0;
  virtual auto remove(const K& key, const V& value) -> void = 0;
  virtual auto query(const QueryCondition& cond) -> std::set<V> = 0;
  
  virtual ~IIndex() = default;
};