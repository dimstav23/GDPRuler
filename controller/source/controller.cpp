#include <iostream>
#include <string>
#include "absl/strings/match.h" // for StartsWith function
#include <chrono>
#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cassert>
#include <functional>

#include "default_policy.hpp"
#include "query.hpp"
#include "query_rewriter.hpp"
#include "gdpr_filter.hpp"
#include "common.hpp"
#include "kv_client/factory.hpp"
#include "logging/logger.hpp"
#include "logging/monitor.hpp"
#include "gdpr_regulator.hpp"

using controller::default_policy;
using controller::cipher_engine;
using controller::query;
using controller::query_rewriter;
using controller::gdpr_filter;
using controller::logger;
using controller::gdpr_monitor;
using controller::gdpr_regulator;

auto handle_get(const std::unique_ptr<kv_client> &client, 
                const query &query_args,
                const default_policy &def_policy) -> std::string 
{
  auto res = client->gdpr_get(query_args.key());
  auto filter = std::make_shared<gdpr_filter>(res);

  // Check if the retrieved value requires logging
  auto monitor = gdpr_monitor(filter, query_args, def_policy);

  bool is_valid = filter->validate(query_args, def_policy);
  // Perform the logging of the (in)valid operation -- if needed
  monitor.monitor_query(is_valid);

  if (is_valid) {
    // if the key exists and complies with the gdpr rules
    // then return the value of the get operation
    return controller::remove_gdpr_metadata(res.value());
  }
  
  return "GET_FAILED: Invalid key or does not comply with GDPR rules";
}

auto handle_put(const std::unique_ptr<kv_client> &client, 
                const query &query_args,
                const default_policy &def_policy) -> std::string 
{
  auto res = client->gdpr_get(query_args.key());
  auto filter = std::make_shared<gdpr_filter>(res);

  bool is_valid = true;
  // if the key does not exist, perform the put
  if (!res) {
    // If no value is returned, check the respective query args
    // If no query args are specified, enforce the default policy for monitoring
    auto monitor = gdpr_monitor(query_args, def_policy);
    // construct the gdpr metadata for the new value
    query_rewriter rewriter(query_args, def_policy, query_args.value());
    // Perform the logging of the valid operation -- if needed
    monitor.monitor_query(is_valid, rewriter.new_value());
    auto ret_val = client->gdpr_put(query_args.key(), rewriter.new_value());

    if (ret_val) {
      return "PUT_SUCCESS";
    }
    return "PUT_FAILED: Failed to put value";
  }

  // if the key exists and complies with the gdpr rules, perform the put
  if ((is_valid = filter->validate(query_args, def_policy))) {
    // Check if the retrieved value requires logging
    // the query args do not need to be checked since they cannot update the 
    // gpdr metadata of the value -- only putm operations can
    auto monitor = gdpr_monitor(filter, query_args, def_policy);
    // update the current value with the new one without modifying any metadata
    query_rewriter rewriter(res.value(), query_args.value());
    // Perform the logging of the valid operation -- if needed
    monitor.monitor_query(is_valid, rewriter.new_value());
    auto ret_val = client->gdpr_put(query_args.key(), rewriter.new_value());

    if (ret_val) {
      return "PUT_SUCCESS";
    }
    return "PUT_FAILED: Failed to put value";
  }
  
  // Perform the logging of the invalid operation -- if needed
  auto monitor = gdpr_monitor(filter, query_args, def_policy);
  monitor.monitor_query(is_valid);
  return "PUT_FAILED: Invalid key or does not comply with GDPR rules";
  
}

auto handle_delete(const std::unique_ptr<kv_client> &client, 
                  const query &query_args,
                  const default_policy &def_policy) -> std::string 
{
  auto res = client->gdpr_get(query_args.key());
  auto filter = std::make_shared<gdpr_filter>(res);
  // Check if the retrieved value requires logging
  auto monitor = gdpr_monitor(filter, query_args, def_policy);
  bool is_valid = filter->validate(query_args, def_policy);
  // Perform the logging of the (in)valid operation -- if needed
  monitor.monitor_query(is_valid);
  
  if (is_valid) {
    // if the key exists and complies with the gdpr rules
    // then perform the delete operation
    auto ret_val = client->gdpr_del(query_args.key());

    if (ret_val) {
      return "DELETE_SUCCESS";
    }
    return "DELETE_FAILED: Failed to delete key";
  }

  return "DELETE_FAILED: Invalid key or does not comply with GDPR rules";
}

auto handle_get_logs(const query &query_args,
                     const default_policy &def_policy) -> std::string 
{

  /* if the current key does not match with the regulator key, return */
  if (!gdpr_regulator::validate_reg_key(query_args, def_policy)) {
    // std::cout << "getLogs query requested without the regulator key." << std::endl;
    return "GET_LOGS_FAILED: Invalid regulator key";
  }

  auto regulator = gdpr_regulator(); 
  std::stringstream response;

  if (query_args.log_key() == "read_all") {
    // std::cout << "Reading all the log files..." << std::endl;
    response << "Reading all the log files:" << std::endl;
    std::vector<std::string> log_files = regulator.retrieve_logs();
    // TODO: redirect this output to the regulator secure channel
    for (const auto& log : log_files) {
      // std::cout << "Log file: " << log << std::endl;
      response << "Log file: " << log << std::endl;
      std::vector<std::string> log_entries = regulator.read_log(log);
      for (const auto& entry : log_entries) {
        // std::cout << entry << std::endl;
        response << entry << std::endl;
      }
    }
  }
  else if (query_args.log_key() == "dir") {
    // std::cout << "Available log files:" << std::endl;
    response << "Available log files:" << std::endl;
    std::vector<std::string> log_files = regulator.retrieve_logs();
    // TODO: redirect this output to the regulator secure channel
    for (const auto& log : log_files) {
      // std::cout << log << std::endl;
      response << log << std::endl;
    }
  }
  else {
    // std::cout << "Reading the log file of key " << query_args.log_key() << ":" << std::endl;
    response << "Reading the log file of key " << query_args.log_key() << ":" << std::endl;
    std::vector<std::string> log_entries = regulator.read_key_log(query_args.log_key());
    // TODO: redirect this output to the regulator secure channel
    for (const auto& entry : log_entries) {
      // std::cout << entry << std::endl;
      response << entry << std::endl;
    }
  }

  return response.str();
}

auto handle_connection
(int socket, const std::string& db_type, const std::string& db_address, const default_policy& def_policy) -> void 
{
  std::array<char, max_msg_size> buffer{};
  
  // create the connection with the database instance
  std::unique_ptr<kv_client> client = kv_factory::create(db_type, db_address);

  while (true) {
    // Read data from the socket
    ssize_t bytes_read = recv(socket, buffer.data(), buffer.size() - 1, 0);
    if (bytes_read <= 0) {
      // Failed to read from socket or connection closed
      break;
    }
    // Ensure non-negative value for valid length
    auto valid_length = static_cast<std::array<char, max_msg_size>::size_type>(bytes_read);
    if (valid_length >= buffer.size() - 1) {
      valid_length = buffer.size() - 1;
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
    buffer[valid_length] = '\0';

    const query query_args(buffer.data());
    std::string response;

    if (query_args.cmd() == "exit") [[unlikely]] {
      std::cout << "Client exiting..." << std::endl;
      break;
    }
    else if (query_args.cmd() == "invalid") [[unlikely]] {
      std::cout << "Invalid command" << std::endl;
      response = "Invalid command";
    }
    else [[likely]] {
      if (query_args.cmd() == "get") {
        response = handle_get(client, query_args, def_policy);
      }
      else if (query_args.cmd() == "put") {
        response = handle_put(client, query_args, def_policy);
      }
      else if (query_args.cmd() == "delete") {
        response = handle_delete(client, query_args, def_policy);
      }
      else if (query_args.cmd() == "putm") { /* ignore for now */
        continue;
      }
      else if (query_args.cmd() == "getm") { /* ignore for now */
        continue;
      }
      else if (query_args.cmd() == "delm") { /* ignore for now */
        continue;
      }
      else if (query_args.cmd() == "getlogs") {
        // current client resembles the regulator
        response = handle_get_logs(query_args, def_policy);
      }
      else {
        // std::cout << "Invalid command: " << query_args.cmd() << std::endl;
        response = "Invalid command";
      }
    }

    ssize_t bytes_sent = send(socket, response.c_str(), response.length(), 0);
    if (bytes_sent <= 0) {
      // Failed to send response or connection closed
      break;
    }

    // Send an acknowledgment (ACK) back to the client for now
    // std::string ack_message = "ACK";
    // ssize_t bytes_sent = send(socket, ack_message.c_str(), ack_message.length(), 0);
    // if (bytes_sent <= 0) {
    //   // Failed to send ACK or connection closed
    //   break;
    // }
  }
}

auto main(int argc, char* argv[]) -> int
{ 
  // read the default policy line
  std::string def_policy_line;
  std::getline(std::cin, def_policy_line);
  default_policy def_policy;

  if (absl::StartsWith(def_policy_line, controller::def_policy_prefix)) {
    def_policy = default_policy{def_policy_line};
  } else {
    std::cout << "Invalid default policy provided\n";
    return 1;
  }

  /* initialize the client object that exports put/get/delete API */
  auto args = std::span(argv, static_cast<size_t>(argc));
  std::string db_type = get_command_line_argument(args, "--db");
  if (db_type.empty()) {
    std::cerr << "--db {redis,rocksdb} argument is not passed!" << std::endl;
    std::quick_exit(1);
  }
  std::string db_address = get_command_line_argument(args, "--db_address");
  
  // set the log path based on the input parameter
  const std::string log_path = get_command_line_argument(args, "--logpath");
  logger::get_instance()->init_log_path(log_path);

  // set the database encryption key based on the input parameter
  const std::string db_encryption_key = get_command_line_argument(args, "--db_encryptionkey");
  if (!cipher_engine::get_instance()->init_encryption_key(db_encryption_key, cipher_key_type::db_key)) {
    std::cerr << "cipher engine error at the db encryption key init phase" << std::endl;
    std::quick_exit(1);
  }

  // set the log encryption key based on the input parameter
  const std::string log_encryption_key = get_command_line_argument(args, "--log_encryptionkey");
  if (!cipher_engine::get_instance()->init_encryption_key(log_encryption_key, cipher_key_type::log_key)) {
    std::cerr << "cipher engine error at the log encryption key init phase" << std::endl;
    std::quick_exit(1);
  }

  // Create a socket and accept for clients
  std::string frontend_address = get_command_line_argument(args, "--frontend_address");
  std::string frontend_port = get_command_line_argument(args, "--frontend_port");

  int listen_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_socket == -1) {
    std::cerr << "Failed to create socket" << std::endl;
    return 1;
  }
  struct sockaddr_in server_address{};
  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = inet_addr(frontend_address.c_str());
  server_address.sin_port = htons(static_cast<uint16_t>(std::stoi(frontend_port)));

  // Bind the socket to the address and port
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  if (bind(listen_socket, reinterpret_cast<struct sockaddr*>(&server_address), sizeof(server_address)) == -1) {
    std::cerr << "Failed to bind socket to address" << std::endl;
    close(listen_socket);
    return 1;
  }

  // Start listening for incoming connections
  if (listen(listen_socket, SOMAXCONN) == -1) {
    std::cerr << "Failed to listen for connections" << std::endl;
    close(listen_socket);
    return 1;
  }

  while (true) {
    // Accept an incoming connection
    struct sockaddr_in client_address{};
    socklen_t client_address_length = sizeof(client_address);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    int client_socket = accept4(listen_socket, reinterpret_cast<struct sockaddr*>(&client_address), 
                                &client_address_length, SOCK_CLOEXEC);
    if (client_socket == -1) {
      std::cerr << "Failed to accept connection" << std::endl;
      break;
    }

    // Create a new thread and pass the client socket to it
    std::thread connection_thread(handle_connection, client_socket, db_type, db_address, def_policy);
    connection_thread.detach();  // Detach the thread and let it run independently
  }

  return 0;
}
