#include <iostream>
#include <string>
#include <memory>
#include "kv_client/factory.hpp"
#include "query.hpp"
#include "common.hpp"

#ifdef DEBUG
#include <chrono>
#endif

using controller::query;

auto handle_get(const query &query_args, std::unique_ptr<kv_client> &client) -> std::string
{
  auto ret_val = client->gdpr_get(query_args.key());
  if (ret_val) {
    return ret_val.value();
  } 
  return GET_FAILED; // GET_FAILED: Non existing key
}

auto handle_put(const query &query_args, std::unique_ptr<kv_client> &client) -> std::string
{
  bool success = client->gdpr_put(query_args.key(), query_args.value());
  if (success) {
    return PUT_SUCCESS;
  } 
  return PUT_FAILED; // PUT_FAILED: Failed to put value
}

auto handle_delete(const query &query_args, std::unique_ptr<kv_client> &client) -> std::string
{
  bool success = client->gdpr_del(query_args.key());
  if (success) {
    return DELETE_SUCCESS;
  }
  return DELETE_FAILED; // DELETE_FAILED: Failed to delete key
}

auto main(int argc, char* argv[]) -> int
{ 
  auto args = std::span(argv, static_cast<size_t>(argc));
  std::string db_type = get_command_line_argument(args, "--db");
  if (db_type.empty()) {
    std::cerr << "--db {redis,rocksdb} argument is not passed!" << std::endl;
    std::quick_exit(1);
  }
  std::string db_address = get_command_line_argument(args, "--db_address");

  // Create the connection with the database instance
  std::unique_ptr<kv_client> client = kv_factory::create(db_type, db_address);

  #ifdef DEBUG
  std::chrono::duration<double> total_query_time{};
  #endif

  std::string input;
  while (true) {
    // std::cout << "Enter command (get/put/delete/exit): ";
    std::getline(std::cin, input);

    #ifdef DEBUG
    auto start_time = std::chrono::high_resolution_clock::now();
    #endif
    
    const query query_args(input);
    std::string result;
    if (query_args.cmd() == "get") {
      result = handle_get(query_args, client);
    } else if (query_args.cmd() == "put") {
      result = handle_put(query_args, client);
    } else if (query_args.cmd() == "delete") {
      result = handle_delete(query_args, client);
    } else if (query_args.cmd() == "exit") {
      std::cout << "Exiting..." << std::endl;
      break;
    } else {
      result = INVALID_COMMAND;
    }

    #ifdef DEBUG
    auto end_time = std::chrono::high_resolution_clock::now();
    auto query_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    total_query_time += query_time;
    #endif

    std::cout << result << std::endl;
  }

  #ifdef DEBUG
  std::cerr << "Total query processing time: " << total_query_time.count() << " seconds\n";
  #endif
  
  return 0;
}
