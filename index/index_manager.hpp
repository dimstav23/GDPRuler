#pragma once

#include "string"
#include "set"

#include "index_factory.hpp"

// Facade pattern: provides a simple insert/remove/query APIs
template<typename V>
class IndexManager {
public:

  auto insert(const std::string& field, const std::string& value, const V& record_id) -> void {
    // TODO: insert data to each data structure
    auto qc = QueryCondition::equality(field, value);
    auto& idx = IndexFactory<V>::getInstance().getIndex(qc);
    idx.insert(value, record_id);
  }

  void remove(const std::string& field,const std::string& value, const V& record_id) {
    // TODO: insert data to each data structure
    auto qc = QueryCondition::equality(field, value);
    auto& idx = IndexFactory<V>::getInstance().getIndex(qc);
    idx.remove(value, record_id);
  }

  auto query(const QueryCondition& qc) -> std::set<V>{
    auto& idx = IndexFactory<V>::getInstance().getIndex(qc);
    return idx.query(qc);
  }

};