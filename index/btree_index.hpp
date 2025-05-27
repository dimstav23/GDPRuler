#pragma once


#include <set>
#include <string>

#include "iindex.hpp"
#include "query_condition.hpp"
#include "bplus_tree.hpp"


template<typename K, typename V>
class BTreeIndex: public IIndex<K, V> {
private:
  BPlusTree<K, V> tree_;

  BTreeIndex() = default;
  ~BTreeIndex() = default;
  BTreeIndex(const BTreeIndex&)             = delete; // copy constructor
  BTreeIndex& operator=(const BTreeIndex&)  = delete; // copy assignment operator
  BTreeIndex(BTreeIndex&&)                  = delete; // move constructor
  BTreeIndex& operator=(BTreeIndex&&)       = delete; // move assignment

public:
  // TODO: create an index for each field, hence remove this
  // Singleton Pattern
  static auto getInstance() -> BTreeIndex& {
    static BTreeIndex inst;
    return inst;
  }

  auto insert(const K& key, const V& value) -> void override {
    tree_.insert(key, value);
  }

  auto remove(const K& key, const V& value) -> void override {
    // tree_.remove(key, value);
  }
    
  auto query(const QueryCondition& cond) -> std::set<V> override {
    return tree_.search(cond.eq_value);
  }
};