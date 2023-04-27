#include <cstring>
#include <chrono>
#include <iostream>

#include "default_policy.hpp"
#include "gdpr_filter.hpp"
#include "query.hpp"
#include "logging/logger.hpp"

using controller::default_policy;
using controller::logger;
using controller::query;

auto main() -> int
{
  // default policy is only used for the user_key, so we don't mind
  // about its instantiation parameters
  auto const def_policy = default_policy {};
  // get the logger instance
  logger* m_history_logger = logger::get_instance();
  
  constexpr int value_size = 1024;
  // NOLINTNEXTLINE(cert-err58-cpp)
  const std::string dummy_value(value_size, 'x');

  // Run each function 1000000 times and measure the time taken
  const int iterations = 1000000;

  // query args should have the following:
  // 1. the user_key
  // 2. the key
  // 3. the cmd
  std::string key1 = "key1";
  std::string user_key1 = "user1";
  std::string cmd1 = "put";
  auto const query_args1 = query {user_key1, key1, cmd1};

  // Measure the time taken by log_attempt
  auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < iterations; i++) {
    m_history_logger->log_attempt(query_args1, def_policy);
  }
  auto end = std::chrono::steady_clock::now();
  std::chrono::duration<double> duration = end - start;
  std::cout << "log_attempt took " << duration.count() << " seconds." << std::endl;

    // setup the new query
  std::string key2 = "key2";
  std::string user_key2 = "user2";
  std::string cmd2 = "put";
  auto const query_args2 = query {user_key2, key2, cmd2};

  // Measure the time taken by log_result
  start = std::chrono::steady_clock::now();
  for (int i = 0; i < iterations; i++) {
    m_history_logger->log_result(query_args2, def_policy, true, dummy_value);
  }
  end = std::chrono::steady_clock::now();
  duration = end - start;
  std::cout << "log_result took " << duration.count() << " seconds." << std::endl;

  // setup the new query
  std::string key3 = "key3";
  std::string user_key3 = "user3";
  std::string cmd3 = "put";
  auto const query_args3 = query {user_key3, key3, cmd3};

  // Measure the time taken by log_encoded_attempt
  start = std::chrono::steady_clock::now();
  for (int i = 0; i < iterations; i++) {
    m_history_logger->log_encoded_attempt(query_args3, def_policy);
  }
  end = std::chrono::steady_clock::now();
  duration = end - start;
  std::cout << "log_encoded_attempt took " << duration.count() << " seconds." << std::endl;

  // setup the new query
  std::string key4 = "key4";
  std::string user_key4 = "user4";
  std::string cmd4 = "put";
  auto const query_args4 = query {user_key4, key4, cmd4};

  // Measure the time taken by log_encoded_result
  start = std::chrono::steady_clock::now();
  for (int i = 0; i < iterations; i++) {
    m_history_logger->log_encoded_result(query_args4, def_policy, true, dummy_value);
  }
  end = std::chrono::steady_clock::now();
  duration = end - start;
  std::cout << "log_encoded_result took " << duration.count() << " seconds." << std::endl;

  return 0;
}
