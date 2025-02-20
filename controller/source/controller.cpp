#include <iostream>
#include <string>
#include "absl/strings/match.h" // for StartsWith function
#include <thread>
#include <cassert>
#include <functional>
#include <sys/mman.h>

#include "default_policy.hpp"
#include "query.hpp"
#include "query_rewriter.hpp"
#include "gdpr_filter.hpp"
#include "common.hpp"
#include "kv_client/factory.hpp"
#include "logging/logger.hpp"
#include "logging/monitor.hpp"
#include "gdpr_regulator.hpp"

#ifdef DEBUG
#include <chrono>
#endif

using controller::default_policy;
using controller::cipher_engine;
using controller::query;
using controller::query_rewriter;
using controller::gdpr_filter;
using controller::logger;
using controller::gdpr_monitor;
using controller::gdpr_regulator;

// Declare a thread-local default_policy object
thread_local default_policy def_policy;

auto receive_policy(int socket) -> std::optional<default_policy>
{
  // Allocate a large buffer using mmap to hold the message and its size
  void* buffer = mmap(nullptr, max_msg_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (buffer == MAP_FAILED) {
    std::cerr << "Failed to allocate buffer" << std::endl;
    return std::nullopt;
  }

  // Read the policy from the socket
  ssize_t bytes_read = safe_sock_receive(socket, buffer);
  if (bytes_read <= 0) {
    std::cerr << "Failed to read the message or the connection is closed." << std::endl;
    return std::nullopt;
  }
  // Set the termination character for the string
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  (static_cast<char*>(buffer))[bytes_read] = '\0';

  std::string client_policy(static_cast<char*>(buffer));

  // Send acknowledgment for policy receive
  std::string ack = "ACK\0";
  // Send the response to the client
  ssize_t bytes_sent = safe_sock_send(socket, ack.data(), ack.length());
  if (bytes_sent <= 0) {
    std::cerr << "Failed to send the response to the client or the connection is closed." << std::endl;
    return std::nullopt;
  }

  return default_policy(client_policy);
}

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
    return controller::remove_gdpr_metadata(std::move(res.value()));
  }
  
  return GET_FAILED;// GET_FAILED: Non existing key or does not comply with GDPR rules;
}

auto handle_put(const std::unique_ptr<kv_client> &client, 
                const query &query_args,
                const default_policy &def_policy) -> std::string 
{
  auto res = client->gdpr_get(query_args.key());

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
      return PUT_SUCCESS;
    }
    return PUT_FAILED; //PUT_FAILED: Failed to put value
  }

  // if the key exists and complies with the gdpr rules, perform the put
  auto filter = std::make_shared<gdpr_filter>(res);
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
      return PUT_SUCCESS;
    }
    return PUT_FAILED; // PUT_FAILED: Failed to put value
  }
  
  // Perform the logging of the invalid operation -- if needed
  auto monitor = gdpr_monitor(filter, query_args, def_policy);
  monitor.monitor_query(is_valid);
  return PUT_FAILED; // PUT_FAILED: Invalid key or does not comply with GDPR rules
  
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
      return DELETE_SUCCESS;
    }
    return DELETE_FAILED; // DELETE_FAILED: Failed to delete key
  }

  return "DELETE_FAILED: Invalid key or does not comply with GDPR rules";
}

auto handle_get_metadata(const std::unique_ptr<kv_client> &client,
                const query &query_args,
                const default_policy &def_policy) -> std::string
{
  auto res = client->gdpr_getm(query_args.key());
  auto filter = std::make_shared<gdpr_filter>(res);

  // Check if the retrieved key requires logging
  auto monitor = gdpr_monitor(filter, query_args, def_policy);
  bool is_valid = filter->validate(query_args, def_policy);
  // Perform the logging of the (in)valid operation -- if needed
  monitor.monitor_query(is_valid);
  if (is_valid) {
    // if the key exists and complies with the gdpr rules
    // then return the GDPR metadata of the key
    return controller::preserve_only_gdpr_metadata(std::move(res.value()));
  }

  return GETM_FAILED; // GETM_FAILED: Invalid key or does not comply with GDPR rules
}

auto handle_put_metadata(const std::unique_ptr<kv_client> &client,
                const query &query_args,
                const default_policy &def_policy) -> std::string
{
  auto res = client->gdpr_get(query_args.key());

  bool is_valid = true;
  // if the key does not exist, return the error
  if (!res) {
    return "PUTM_FAILED: The specified key does not exist";
  }
  // if the key exists and complies with the gdpr rules, perform the GDPR metadata update
  auto filter = std::make_shared<gdpr_filter>(res);
  if ((is_valid = filter->validate(query_args, def_policy))) {
    // Check if the retrieved value requires logging
    // the query args do not need to be checked since they cannot update the
    // gpdr metadata of the value -- only putm operations can
    auto monitor = gdpr_monitor(filter, query_args, def_policy);
    // update the current value with the new one without modifying any metadata
    query_rewriter rewriter(res.value(), query_args);
    // Perform the logging of the valid operation -- if needed
    monitor.monitor_query(is_valid, rewriter.new_value());
    auto ret_val = client->gdpr_putm(query_args.key(), rewriter.new_value());
    if (ret_val) {
      return PUTM_SUCCESS;
    }
    return PUTM_FAILED; // PUTM_FAILED: Failed to put value
  }

  // Perform the logging of the invalid operation -- if needed
  auto monitor = gdpr_monitor(filter, query_args, def_policy);
  monitor.monitor_query(is_valid);

  return PUTM_FAILED; // PUTM_FAILED: Invalid key or does not comply with GDPR rules
}

auto handle_get_logs(const query &query_args,
                     const default_policy &def_policy) -> std::string 
{

  /* if the current key does not match with the regulator key, return */
  if (!gdpr_regulator::validate_reg_key(query_args, def_policy)) {
    // std::cout << "getLogs query requested without the regulator key." << std::endl;
    return GET_LOGS_FAILED; // GET_LOGS_FAILED: Invalid regulator key
  }

  auto regulator = gdpr_regulator(); 
  std::stringstream response;

  if (query_args.log_key() == "read_all") {
    response << "Reading all the log files:" << std::endl;
    std::vector<std::string> log_files = regulator.retrieve_logs();
    // TODO: redirect this output to the regulator secure channel
    for (const auto& log : log_files) {
      response << "Log file: " << log << std::endl;
      std::vector<std::string> log_entries = regulator.read_log(log);
      for (const auto& entry : log_entries) {
        response << entry << std::endl;
      }
    }
  }
  else if (query_args.log_key() == "dir") {
    response << "Available log files:" << std::endl;
    std::vector<std::string> log_files = regulator.retrieve_logs();
    // TODO: redirect this output to the regulator secure channel
    for (const auto& log : log_files) {
      response << log << std::endl;
    }
  }
  else {
    response << "Reading the log file of key " << query_args.log_key() << ":" << std::endl;
    std::vector<std::string> log_entries = regulator.read_key_log(query_args.log_key());
    // TODO: redirect this output to the regulator secure channel
    for (const auto& entry : log_entries) {
      response << entry << std::endl;
    }
  }

  return response.str();
}

auto handle_connection
(int socket, const std::string& db_type, const std::string& db_address) -> void
{
  // Receive and set the client-specific policy
  auto received_policy = receive_policy(socket);
  if (received_policy) {
    def_policy = *received_policy;
  } else {
    std::cerr << "Failed to receive client policy." << std::endl;
    safe_close_socket(socket);
    return;
  }

  // Create the connection with the database instance
  std::unique_ptr<kv_client> client = kv_factory::create(db_type, db_address);

  // Allocate a large buffer using mmap to hold the message and its size
  void* buffer = mmap(nullptr, max_msg_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (buffer == MAP_FAILED) {
    std::cerr << "Failed to allocate buffer" << std::endl;
    return;
  }

  #ifdef DEBUG
  std::chrono::duration<double> total_query_time{};
  #endif

  while (true) {
    // Read the message size from the socket
    ssize_t bytes_read = safe_sock_receive(socket, buffer);
    if (bytes_read <= 0) {
      std::cerr << "Failed to read the message or the connection is closed." << std::endl;
      break;
    }
    
    // Set the termination character for the string
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    (static_cast<char*>(buffer))[bytes_read] = '\0';

    #ifdef DEBUG
    auto start_time = std::chrono::high_resolution_clock::now();
    #endif

    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    query query_args(static_cast<char*>(buffer));
    std::string response;

    if (query_args.cmd() == "exit") [[unlikely]] {
      std::cout << "Client exiting..." << std::endl;
      break;
    }
    else if (query_args.cmd() == "invalid") [[unlikely]] {
      response = INVALID_COMMAND;
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
        response = handle_put_metadata(client, query_args, def_policy);
      }
      else if (query_args.cmd() == "getm") { /* ignore for now */
        response = handle_get_metadata(client, query_args, def_policy);
      }
      else if (query_args.cmd() == "getlogs") {
        // current client resembles the regulator
        response = handle_get_logs(query_args, def_policy);
      }
      else {
        response = INVALID_COMMAND;
      }
    }

    #ifdef DEBUG
    auto end_time = std::chrono::high_resolution_clock::now();
    auto query_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    total_query_time += query_time;
    #endif

    // Check the message size
    size_t response_length = response.length();
    if (response_length > max_msg_size) {
      std::cerr << "Outgoing message too large." << std::endl;
      break;
    }

    // Send the response to the client
    ssize_t bytes_sent = safe_sock_send(socket, response.data(), response_length);
    if (bytes_sent <= 0) {
      std::cerr << "Failed to send the response to the client or the connection is closed." << std::endl;
      break;
    }
  }

  #ifdef DEBUG
  std::cout << "Total query processing time: " << total_query_time.count() << " seconds\n";
  #endif

  // Unmap the socket communication buffer
  munmap(buffer, max_msg_size);
  // Close the client socket
  safe_close_socket(socket);
}

auto main(int argc, char* argv[]) -> int
{ 
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
  std::string controller_address = get_command_line_argument(args, "--controller_address");
  std::string controller_port = get_command_line_argument(args, "--controller_port");

  int listen_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_socket == -1) {
    std::cerr << "Failed to create socket" << std::endl;
    return 1;
  }

  // Enable SO_REUSEADDR option
  int reuse = 1;
  if (setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1) {
    std::cerr << "Failed to set SO_REUSEADDR option" << std::endl;
    safe_close_socket(listen_socket);
    return 1;
  }

  int tcpnodelay = 1;
  if (setsockopt(listen_socket, IPPROTO_TCP, TCP_NODELAY, &tcpnodelay, sizeof(tcpnodelay))) {
    std::cerr << "Failed to set TCP_NODELAY option" << std::endl;
    safe_close_socket(listen_socket);
    return 1;
  }

  // Setup the frontend server (controller) socket
  struct sockaddr_in server_address{};
  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = inet_addr(controller_address.c_str());
  server_address.sin_port = htons(static_cast<uint16_t>(std::stoi(controller_port)));

  // Bind the socket to the address and port
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  if (bind(listen_socket, reinterpret_cast<struct sockaddr*>(&server_address), sizeof(server_address)) == -1) {
    std::cerr << "Failed to bind socket to address" << std::endl;
    safe_close_socket(listen_socket);
    return 1;
  }

  // Start listening for incoming connections
  if (listen(listen_socket, SOMAXCONN) == -1) {
    std::cerr << "Failed to listen for connections" << std::endl;
    safe_close_socket(listen_socket);
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
    // The client socket must be independently managed by the thread now
    std::thread connection_thread(handle_connection, client_socket, db_type, db_address);
    connection_thread.detach();  // Detach the thread and let it run independently
  }
  
  return 0;
}
