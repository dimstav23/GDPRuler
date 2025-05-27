#pragma once


#include <string>
#include <unordered_map>
#include <functional>

#include "query_condition.hpp"
#include "iindex.hpp"
#include "btree_index.hpp"
#include "inverted_index.hpp"


// A registry that maps QueryCondition::type -> IIndex strategy (callable fn that returns an iindex instance)
template<typename V>
class IndexFactory {
private:
//TODO
  std::unordered_map<ConditionType, std::function<IIndex<std::string, V>&()>> registry_;

  IndexFactory() = default;
   ~IndexFactory() = default;
  IndexFactory(const IndexFactory&)             = delete; // copy constructor
  IndexFactory& operator=(const IndexFactory&)  = delete; // copy assignment operator
  IndexFactory(IndexFactory&&)                  = delete; // move constructor
  IndexFactory& operator=(IndexFactory&&)       = delete; // move assignment
  
  // Factory pattern
  auto getFactory(ConditionType type) -> std::function<IIndex<std::string, V>&()> {
    switch (type) {
      case ConditionType::Equality:
      // TODO
        // return &InvertedIndex<V>::getInstance;
      return &BTreeIndex<std::string, V>::getInstance;
      case ConditionType::Range:
        return &BTreeIndex<std::string, V>::getInstance;
      default:
        throw std::invalid_argument("Unknown condition type");
    }
  }

public:
  // Singleton Pattern
  static auto getInstance() -> IndexFactory& {
    static IndexFactory inst;
    return inst;
  }
  
  // TODO: use fields (Inverted index for only purpose, only user)
  auto getIndex(const QueryCondition& qc) -> IIndex<std::string,V>& {
    auto it = registry_.find(qc.type);
    if (it == registry_.end()) {
      registry_[qc.type] = getFactory(qc.type);
      it = registry_.find(qc.type);
    }
    return it->second(); // call the fn
  }

};