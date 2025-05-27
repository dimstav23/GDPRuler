#include <iostream>
#include <random>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <cassert>

#include "index_manager.hpp"
#include "query_condition.hpp"

int main(){
  constexpr int NUM_USERS     = 100;
  constexpr int OPS_PER_USER  = 1000;
  constexpr int SEED          = 10;

  // 1) Deterministic RNG
  std::mt19937_64 rnd(SEED);
  std::uniform_int_distribution<int> opDist(0, 2);  // 0=insert, 1=query, 2=remove

  IndexManager<std::string> mgr;

  // 2) Ground‐truth: for each user string, which record IDs are active
  std::map<std::string, std::set<std::string>> truth;

  // 3) Generate user names: "user0", "user1", ..., "user99"
  std::vector<std::string> users;
  users.reserve(NUM_USERS);
  for(int i = 0; i < NUM_USERS; ++i)
      users.push_back("user" + std::to_string(i));

  int nextRecordNum = 0;

  // 4) Run operations
  std::uniform_int_distribution<int> userDist(0, NUM_USERS - 1);

  int totalOps = NUM_USERS * OPS_PER_USER;
  for(int i = 0; i < totalOps; ++i) {
    // pick any user at random
    int ui = userDist(rnd);
    const auto& user = users[ui];

    // pick operation
    int choice = opDist(rnd);

    if (choice == 0) {
      // INSERT for this random user
      std::string recId = "rec" + std::to_string(nextRecordNum++);
      mgr.insert("user", user, recId);
      truth[user].insert(recId);
    }
    else if (choice == 1) {
       // QUERY for this random user
      auto res = mgr.query(QueryCondition::equality("user", user));

      // verify against ground truth
      assert(res == truth[user]);
      
    } else { 
      // REMOVE for this random user
      auto &s = truth[user];
      if (!s.empty()) {
        auto it = s.begin();
        std::advance(it, rnd() % s.size());
        std::string recId = *it;
        
        mgr.remove("user", user, recId);
        s.erase(it);
      }
    }
  }

  std::cout << "All user‐index tests passed ("
            << nextRecordNum << " inserts total).\n";
  return 0;
};
