#include <string>
#include <iostream>
#include <functional>



#include "query_condition.hpp"
#include "index_manager.hpp"

#include "bplus_tree.hpp"
auto test() -> void {

  BPlusTree<std::string, std::string> tree;

  tree.insert("k1", "v10");
  tree.insert("k1", "v11");
  tree.insert("k2", "v20");
  tree.insert("k3", "v30");
  tree.print_tree();
  std::cout << std::endl;

  tree.insert("k4", "v40");
  tree.print_tree();
  std::cout << std::endl;

  tree.insert("k5", "v50");
  tree.insert("k6", "v60");
  tree.print_tree();
  std::cout << std::endl;

  tree.insert("k7", "v70");
  tree.insert("k8", "v80");
  tree.print_tree();
  std::cout << std::endl;

  auto s1 = tree.search("k1");
  std::cout << "{ ";
    for (const auto& x : s1) {
        std::cout << x << " ";
    }
    std::cout << "}\n";

  auto s2 = tree.range_query("k3", "k7");
  std::cout << "{ ";
    for (const auto& x : s2) {
        std::cout << x << " ";
    }
    std::cout << "}\n";

  tree.remove("k7");
  tree.print_tree();
  std::cout << std::endl;

  tree.remove("k8");
  tree.print_tree();
  std::cout << std::endl;

  tree.remove("k3");
  tree.print_tree();
  std::cout << std::endl;

}

auto main(int argc, char* argv[]) -> int
{

  test();
  /*
  IndexManager<std::string> m;
  m.insert("user", "user1", "k1");
  m.insert("user", "user1", "k4");
  m.insert("user", "user1", "k6");
  m.insert("user", "user2", "k20");
  m.insert("user", "user3", "k30");
  m.insert("user", "user4", "k40");
  m.insert("user", "user5", "k50");
  m.insert("user", "user6", "k60");
  
  auto s = m.query(QueryCondition::equality("user", "user1"));
  for (const auto& v : s) {
    std::cout << v << " ";
  }
  std::cout << std::endl;
  
  m.remove("user", "user2", "");

  s = m.query(QueryCondition::range("user", "user2", "user9"));
  for (const auto& v : s) {
    std::cout << v << " ";
  }
  std::cout << std::endl;

  */

  return 0;
}