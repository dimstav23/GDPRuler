#pragma once


#include <algorithm>
#include <iostream>
#include <memory>
#include <mutex>
#include <set>
#include <vector>


template<typename K, typename V, size_t ORDER = 64>
class BPlusTree {
private:
  
  // property: leaf nodes contain between ⌈m/2⌉ and m keys
  static constexpr size_t MIN_KEYS_LEAF = (ORDER+1)/2; // ceil(ORDER/2)
  static constexpr size_t MAX_KEYS_LEAF = ORDER;

  // property: internal nodes except the root contain between ⌈m/2⌉ and m children
  static constexpr size_t MIN_CHILDREN_INTERNAL = (ORDER+1)/2; // ceil(ORDER/2)
  static constexpr size_t MAX_CHILDREN_INTERNAL = ORDER;
  
  // property: each internal node with k children contains k-1 keys
  static constexpr size_t MIN_KEYS_INTERNAL = MIN_CHILDREN_INTERNAL - 1;
  static constexpr size_t MAX_KEYS_INTERNAL = MAX_CHILDREN_INTERNAL - 1;

  struct Node {
    bool is_leaf;
    std::vector<K> keys;
    std::vector<std::unique_ptr<Node>> children;   // internal
    std::vector<std::vector<V>> values;            // leaf // TODO: std::vector<std::set<V>>
    Node* next;

    Node(bool leaf = false) 
      : is_leaf(leaf), 
        next(nullptr) {
      // pre-allocate capcity
      if (leaf) {
        keys.reserve(MAX_KEYS_LEAF);
        values.reserve(MAX_KEYS_LEAF);
      } else {
        keys.reserve(MAX_KEYS_INTERNAL);
        children.reserve(MAX_CHILDREN_INTERNAL);
      }
    }
  };

  std::unique_ptr<Node> root_;

public:
  BPlusTree() : root_(std::make_unique<Node>(true)) {}
  
  // recursive cleanup
  ~BPlusTree() = default;

  /**
   *  [ko, k1, k2, ..., km-1]
   *  [c0, c1, c2, ..., cm-1, cm]
   *  
   *  ci -> [ki-1, ki)
   */
  auto search(const K& key) -> std::set<V> {
    
    Node* n = root_.get();
    
    while (!n->is_leaf) {
      // key < *it 
      auto it = std::upper_bound(n->keys.begin(), n->keys.end(), key);
      n = n->children[it - n->keys.begin()].get();
    }

    // key <= *it
    auto it = std::lower_bound(n->keys.begin(), n->keys.end(), key);

    if (it == n->keys.end() || *it != key) return {};

    size_t idx = it - n->keys.begin();
    return std::set<V>(n->values[idx].begin(), n->values[idx].end());
  }

  auto range_query(const K& low, const K& high) -> std::vector<V> {
    
    std::vector<V> result;
    Node* n = root_.get();
    
    if(high < low) return result;

    while (!n->is_leaf) {
      // key < *it 
      auto it = std::upper_bound(n->keys.begin(), n->keys.end(), low);
      n = n->children[it - n->keys.begin()].get();
    }

    // key <= *it
    auto it = std::lower_bound(n->keys.begin(), n->keys.end(), low);
    size_t idx = it - n->keys.begin();

    while (n) {
      for (size_t i = idx; i < n->keys.size() && n->keys[i] <= high; ++i) {
          result.insert(
              result.end(),
              n->values[i].begin(),
              n->values[i].end()
          );
      }

      if (n->keys.empty() || n->keys.back() > high) break;
      
      n = n->next;
      idx = 0;
    }

    return result;      
  };

  auto insert(const K& key, const V& value) -> void {
    auto [new_sibling, promo_key] = insert_rec(root_.get(), key, value);

    // if root split, make a new root
    if (new_sibling) {
      auto new_root = std::make_unique<Node>(false);
      new_root->keys.push_back(promo_key);
      // property: the root has at least two children if it is not a leaf node
      new_root->children.push_back(std::move(root_));
      new_root->children.push_back(std::move(new_sibling));
      root_ = std::move(new_root);
    }
  }
  // TODO
  auto remove(const K& key) -> void {
    // special‐case: single‐leaf root
    if (root_->is_leaf) {
      //  key <= *it
      auto it = std::lower_bound(root_->keys.begin(), root_->keys.end(), key);
      if (it != root_->keys.end() && *it == key) {
        size_t idx = it - root_->keys.begin();
        root_->keys.erase(it);
        root_->values.erase(root_->values.begin() + idx);
      }
    } else {
      remove_rec(nullptr, root_.get(), 0, key);
    }

    // collapse root if it lost all keys and has one child
    if (!root_->is_leaf && root_->keys.empty() && root_->children.size() == 1) {
      root_ = std::move(root_->children.front());
    }
  }
  
  // for testing
  auto print_tree(std::ostream& out = std::cout) const -> void {
    print_node(out, root_.get(), 0);
  }
  
private:

  // return {sibling, promoted_key} on split
  auto insert_rec(Node* n, const K& key, const V& val) -> std::pair<std::unique_ptr<Node>,K> {
    if (n->is_leaf) {
      return insert_into_leaf(n, key, val);
    } else {
      return insert_into_internal(n, key, val);
    }
  }

  auto insert_into_leaf(Node* leaf, const K& key, const V& val) -> std::pair<std::unique_ptr<Node>,K> {
    // key <= *it
    auto it = std::lower_bound(leaf->keys.begin(), leaf->keys.end(), key);
    size_t pos = it - leaf->keys.begin();

    if (it != leaf->keys.end() && *it == key) {
      leaf->values[pos].push_back(val);
    } else {
      leaf->keys.insert(it, key);
      leaf->values.insert(leaf->values.begin() + pos, {val});
    }

    // split if overflow
    if (leaf->keys.size() > MAX_KEYS_LEAF) {
      return split_leaf(leaf);
    }
    return {nullptr, K{}};
  }

  auto insert_into_internal(Node* inode, const K& key, const V& val) -> std::pair<std::unique_ptr<Node>,K> {
    // key < *it
    auto it = std::upper_bound(inode->keys.begin(), inode->keys.end(), key);
    size_t pos = it - inode->keys.begin();

    auto [child_sib, promo] = insert_rec(inode->children[pos].get(), key, val);
    if (child_sib) {
      inode->keys.insert(inode->keys.begin() + pos, promo);
      inode->children.insert(inode->children.begin() + pos + 1, std::move(child_sib));
    }

    // split if overflow
    if (inode->keys.size() > MAX_KEYS_INTERNAL) {
      return split_internal(inode);
    }
    return {nullptr, K{}};
  }

  auto split_leaf(Node* leaf) -> std::pair<std::unique_ptr<Node>,K> {
    size_t total = leaf->keys.size();
    size_t mid = total / 2;  // right‐biased

    auto sib = std::make_unique<Node>(true);
    // [mid..end]
    sib->keys.assign(
      std::make_move_iterator(leaf->keys.begin() + mid),
      std::make_move_iterator(leaf->keys.end())
    );
    sib->values.assign(
      std::make_move_iterator(leaf->values.begin() + mid),
      std::make_move_iterator(leaf->values.end())
    );
    // fix leaf chain
    sib->next    = leaf->next;
    leaf->next   = sib.get();
    // [0..mid-1]
    leaf->keys.resize(mid);
    leaf->values.resize(mid);

    // promote smallest key of sibling
    return { std::move(sib), sib->keys.front() };
  }

  auto split_internal(Node* inode) -> std::pair<std::unique_ptr<Node>,K> {
    size_t total = inode->keys.size();
    size_t mid   = total / 2;

    K up = inode->keys[mid];
    auto sib = std::make_unique<Node>(false);

    // [mid+1..end]
    sib->keys.assign(
      std::make_move_iterator(inode->keys.begin() + mid + 1),
      std::make_move_iterator(inode->keys.end())
    );
    sib->children.assign(
      std::make_move_iterator(inode->children.begin() + mid + 1),
      std::make_move_iterator(inode->children.end())
    );

    inode->keys.resize(mid);
    inode->children.resize(mid + 1);

    return { std::move(sib), up };
  }

  // returns true if parent must rebalance child at idx
  auto remove_rec(Node* parent, Node* node, size_t idx_in_parent, const K& key) -> bool {
    if (node->is_leaf) {
      return remove_from_leaf(parent, node, idx_in_parent, key);
    } else {
      return remove_from_internal(parent, node, idx_in_parent, key);
    }
  }

  // ========== LEAF REMOVAL ==========

  // returns true if parent must rebalance child at idx
  auto remove_from_leaf(Node* parent, Node* leaf, size_t pos, const K& key) -> bool {
    // key <= *it
    auto it = std::lower_bound(leaf->keys.begin(), leaf->keys.end(), key);
    if (it == leaf->keys.end() || *it != key) return false;

    size_t idx = it - leaf->keys.begin();
    leaf->keys.erase(it);
    leaf->values.erase(leaf->values.begin() + idx);

    // underflow
    if (parent && leaf->keys.size() < MIN_KEYS_LEAF) {
      handle_leaf_underflow(parent, pos);
      return true; // parent may need rebalancing
    }
    return false;  // no parent‐rebalance needed
  }

  // ========== INTERNAL NODE REMOVAL ==========

  // returns true if parent must rebalance child at idx 
  auto remove_from_internal(Node* parent, Node* inode, size_t pos, const K& key) -> bool {
    // key < *it
    auto it = std::upper_bound(inode->keys.begin(), inode->keys.end(), key);
    size_t ci = it - inode->keys.begin();

    if (!remove_rec(inode, inode->children[ci].get(), ci, key)) {
      return false;
    }
    // after child deletion+possible merge, this node may underflow
    if (parent && inode->keys.size() < MIN_KEYS_INTERNAL) {
      handle_internal_underflow(parent, pos);
      return true; // parent may need rebalancing
    }
    return false;
  }

  // ========== LEAF UNDERFLOW HANDLING ==========

  auto handle_leaf_underflow(Node* parent, size_t idx) -> void {
    // Try borrow or merge in this order
    if (borrow_leaf_from_left(parent, idx)) return;
    if (borrow_leaf_from_right(parent, idx)) return;
    if (idx > 0) 
      merge_leaf_with_left(parent, idx);
    else if (idx + 1 < parent->children.size())        
      merge_leaf_with_right(parent, idx);
  }

  auto borrow_leaf_from_left(Node* p, size_t i) -> bool {
    if (i == 0) return false;

    auto &L = p->children[i-1], &C = p->children[i];
    if (L->keys.size() <= MIN_KEYS_LEAF) return false;

    C->keys.insert(C->keys.begin(), L->keys.back());
    C->values.insert(
        C->values.begin(),
        std::move(L->values.back())
    );
    L->keys.pop_back(); 
    L->values.pop_back();
    // update parent separator
    p->keys[i-1] = C->keys.front();
    return true;
  }

  auto borrow_leaf_from_right(Node* p, size_t i) -> bool {
    if (i+1 >= p->children.size()) return false;

    auto &C = p->children[i], &R = p->children[i+1];
    if (R->keys.size() <= MIN_KEYS_LEAF) return false;

    C->keys.push_back(R->keys.front());
    C->values.push_back(std::move(R->values.front()));
    R->keys.erase(R->keys.begin());
    R->values.erase(R->values.begin());
    // Update parent separator
    p->keys[i] = R->keys.front();
    return true;
  }

  auto merge_leaf_with_left(Node* p, size_t i) -> void {
    auto &L = p->children[i-1], &C = p->children[i];
    // Append C into L
    L->keys.insert(
        L->keys.end(),
        std::make_move_iterator(C->keys.begin()),
        std::make_move_iterator(C->keys.end())
    );
    L->values.insert(
        L->values.end(),
        std::make_move_iterator(C->values.begin()),
        std::make_move_iterator(C->values.end())
    );
    L->next = C->next;
    // Remove C
    p->keys.erase(p->keys.begin() + (i-1));
    p->children.erase(p->children.begin() + i);
  }

  auto merge_leaf_with_right(Node* p, size_t i) -> void {
    auto &C = p->children[i], &R = p->children[i+1];
    // Append R into C
    C->keys.insert(
        C->keys.end(),
        std::make_move_iterator(R->keys.begin()),
        std::make_move_iterator(R->keys.end())
    );
    C->values.insert(
        C->values.end(),
        std::make_move_iterator(R->values.begin()),
        std::make_move_iterator(R->values.end())
    );
    C->next = R->next;
    // Remove R
    p->keys.erase(p->keys.begin() + i);
    p->children.erase(p->children.begin() + (i+1));
  }

  // ========== INTERNAL NODE UNDERFLOW HANDLING ==========

  auto handle_internal_underflow(Node* parent, size_t idx) -> void {
    if (borrow_internal_from_left(parent, idx))  return;
    if (borrow_internal_from_right(parent, idx)) return;
    if (idx > 0) 
      merge_internal_with_left(parent, idx);
    else if (idx + 1 < parent->children.size())    
      merge_internal_with_right(parent, idx);
  }

  auto borrow_internal_from_left(Node* p, size_t i) -> bool {
    if (i == 0) return false;

    auto &L = p->children[i-1], &C = p->children[i];
    if (L->keys.size() <= MIN_KEYS_INTERNAL) return false;
    
    // pull separator down, move L’s last child up
    C->keys.insert(C->keys.begin(), p->keys[i-1]);
    p->keys[i-1] = L->keys.back();
    L->keys.pop_back();
    C->children.insert(
        C->children.begin(),
        std::move(L->children.back())
    );
    L->children.pop_back();
    return true;
  }

  auto borrow_internal_from_right(Node* p, size_t i) -> bool {
    if (i+1 >= p->children.size()) return false;
    
    auto &C = p->children[i], &R = p->children[i+1];
    if (R->keys.size() <= MIN_KEYS_INTERNAL) return false;
    
    // pull separator down, move R’s first child
    C->keys.push_back(p->keys[i]);
    p->keys[i] = R->keys.front();
    R->keys.erase(R->keys.begin());
    C->children.push_back(std::move(R->children.front()));
    R->children.erase(R->children.begin());
    return true;
  }

  auto merge_internal_with_left(Node* p, size_t i) -> void {
    auto &L = p->children[i-1], &C = p->children[i];
    // pull separator down
    L->keys.push_back(p->keys[i-1]);
    L->keys.insert(
        L->keys.end(),
        std::make_move_iterator(C->keys.begin()),
        std::make_move_iterator(C->keys.end())
    );
    L->children.insert(
        L->children.end(),
        std::make_move_iterator(C->children.begin()),
        std::make_move_iterator(C->children.end())
    );
    p->keys.erase(p->keys.begin() + (i-1));
    p->children.erase(p->children.begin() + i);
  }

  auto merge_internal_with_right(Node* p, size_t i) -> void {
    auto &C = p->children[i], &R = p->children[i+1];
    // pull separator down
    C->keys.push_back(p->keys[i]);
    C->keys.insert(
        C->keys.end(),
        std::make_move_iterator(R->keys.begin()),
        std::make_move_iterator(R->keys.end())
    );
    C->children.insert(
        C->children.end(),
        std::make_move_iterator(R->children.begin()),
        std::make_move_iterator(R->children.end())
    );
    p->keys.erase(p->keys.begin() + i);
    p->children.erase(p->children.begin() + (i+1));
  }

  // for testing
  void print_node(std::ostream& out, Node* n, int depth) const {
    out << std::string(depth * 4, ' ');

    if (n->is_leaf) {
      out << "Leaf [";
      for (size_t i = 0; i < n->keys.size(); ++i) {
        out << n->keys[i] << ":";
        out << "{";
        for (size_t j = 0; j < n->values[i].size(); ++j) {
          out << n->values[i][j];
          if (j+1 < n->values[i].size()) out << ",";
        }
        out << "}";
        if (i+1 < n->keys.size()) out << " | ";
      }
      out << "]\n";
    } else {
      out << "Internal [";
      for (size_t i = 0; i < n->keys.size(); ++i) {
        out << n->keys[i];
        if (i+1 < n->keys.size()) out << " | ";
      }
      out << "]\n";
      // recurse children
      for (size_t i = 0; i < n->children.size(); ++i) {
        print_node(out, n->children[i].get(), depth + 1);
      }
    }
  }
};