#pragma once

#include <set>
#include <string>
#include <unordered_map>

#include "iindex.hpp"
#include "query_condition.hpp"

template<typename V>
class InvertedIndex: public IIndex<std::string, V> {
  private:
   // value -> set of record Keys
    std::unordered_map<std::string, std::set<V>> map_;

    InvertedIndex() = default;
    ~InvertedIndex() = default;
    InvertedIndex(const InvertedIndex&)             = delete; // copy constructor
    InvertedIndex& operator=(const InvertedIndex&)  = delete; // copy assignment operator
    InvertedIndex(InvertedIndex&&)                  = delete; // move constructor
    InvertedIndex& operator=(InvertedIndex&&)       = delete; // move assignment

  public:
    // TODO: create an index for each field, hence remove this
    // Singleton Pattern
    static auto getInstance() -> InvertedIndex& {
      static InvertedIndex inst;
      return inst;
    }

    auto insert(const std::string& key, const V& value) -> void override {
      map_[key].insert(value);
    }

    auto remove(const std::string& key, const V& value) -> void override {
      auto it = map_.find(key);
      
      if (it != map_.end()) {
        it->second.erase(value);
        if (it->second.empty()) map_.erase(it);
      }
    }
    
    auto query(const QueryCondition& cond) -> std::set<V> override {
      auto it = map_.find(cond.eq_value);
      
      return (it != map_.end()) ? it->second : std::set<V>{};
    }

};