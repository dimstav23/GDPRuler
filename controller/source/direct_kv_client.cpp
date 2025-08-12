#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <sys/mman.h>

#include "kv_client/factory.hpp"
#include "query.hpp"
#include "common.hpp"

using controller::query;

inline auto handle_get(const query &query_args, std::unique_ptr<kv_client> &client) -> std::string
{
  std::string_view key = query_args.key();
  auto ret_val = client->gdpr_get(key);
  if (ret_val) {
    return std::move(ret_val.value());
  } 
  return GET_FAILED;
}

inline auto handle_put(const query &query_args, std::unique_ptr<kv_client> &client) -> std::string
{
  std::string_view key = query_args.key();
  std::string_view value = query_args.value();
  bool success = client->gdpr_put(key, value);
  if (success) {
    return PUT_SUCCESS;
  } 
  return PUT_FAILED;
}

inline auto handle_delete(const query &query_args, std::unique_ptr<kv_client> &client) -> std::string
{
  std::string_view key = query_args.key();
  bool success = client->gdpr_del(key);
  if (success) {
    return DELETE_SUCCESS;
  }
  return DELETE_FAILED;
}

auto handle_connection(int socket, const std::string& db_type, const std::string& db_address) -> void
{
  #ifdef INTERNAL_TIMING
  bool is_benchmark_thread = false;
  
  // Skip loading, set start time and count benchmark threads
  if (g_skip_first_connection.exchange(false)) {
    // Loading connection - skip
  } else {
    is_benchmark_thread = true;
    g_active_benchmark_threads.fetch_add(1);
    
    if (!g_timing_started.exchange(true)) {
      std::lock_guard<std::mutex> lock(g_timing_mutex);
      g_start_time = std::chrono::steady_clock::now();
    }
  }
  #endif

  // Allocate a large buffer using mmap to hold the message and its size
  void* buffer = mmap(nullptr, max_msg_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (buffer == MAP_FAILED) {
    std::cerr << "Failed to allocate buffer" << std::endl;
    return;
  }

  // Create database connection for this client thread
  std::unique_ptr<kv_client> client = kv_factory::create(db_type, db_address);
  
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
    
    // Process query
    query query_args(static_cast<char*>(buffer));
    std::string response;
    
    if (query_args.cmd() == "exit") [[unlikely]] {
      #ifdef INTERNAL_TIMING
      if (is_benchmark_thread) {
        // Decrement thread count and check if this is the last one
        int remaining = g_active_benchmark_threads.fetch_sub(1) - 1;
        
        if (remaining == 0) {
          // This is the LAST benchmark thread - send timing
          std::lock_guard<std::mutex> lock(g_timing_mutex);
          g_end_time = std::chrono::steady_clock::now();
          auto duration = std::chrono::duration_cast<std::chrono::duration<double>>(g_end_time - g_start_time);
          response = "Total server processing time (incl. communication): " + std::to_string(duration.count()) + " seconds";
        } else {
          response = "Client exiting";
        }
      } else {
        response = "Loading phase completed";
      }
      #else
      response = "Client exiting";
      #endif
      
      // Send the response before breaking
      size_t response_length = response.length();
      if (response_length <= max_msg_size) {
        ssize_t bytes_sent = safe_sock_send(socket, response.data(), response_length);
      }

      break;
    }
    else if (query_args.cmd() == "invalid") [[unlikely]] {
      response = INVALID_COMMAND;
    }
    else [[likely]] {
      if (query_args.cmd() == "get") {
        response = handle_get(query_args, client);
      }
      else if (query_args.cmd() == "put") {
        response = handle_put(query_args, client);
      }
      else if (query_args.cmd() == "delete") {
        response = handle_delete(query_args, client);
      }
      else {
        response = INVALID_COMMAND;
      }
    }

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
  
  // Unmap the socket communication buffer
  munmap(buffer, max_msg_size);
  // Close the client socket
  safe_close_socket(socket);
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
  std::string socket_path = get_command_line_argument(args, "--socket_path");
  if (socket_path.empty()) {
    socket_path = "/tmp/direct_kv_client.sock";
  }
  
  // Create Unix domain socket
  int listen_socket = socket(AF_UNIX, SOCK_STREAM, 0);
  if (listen_socket == -1) {
    std::cerr << "Failed to create Unix socket" << std::endl;
    return 1;
  }
  
  // Remove existing socket file
  unlink(socket_path.c_str());
  
  // Setup Unix socket address
  struct sockaddr_un server_addr{};
  server_addr.sun_family = AF_UNIX;
  strncpy(server_addr.sun_path, socket_path.c_str(), sizeof(server_addr.sun_path) - 1);
  
  // Bind socket
  if (bind(listen_socket, reinterpret_cast<struct sockaddr*>(&server_addr), sizeof(server_addr)) == -1) {
    std::cerr << "Failed to bind Unix socket to " << socket_path << std::endl;
    close(listen_socket);
    return 1;
  }
  
  // Start listening
  if (listen(listen_socket, SOMAXCONN) == -1) {
    std::cerr << "Failed to listen on Unix socket" << std::endl;
    close(listen_socket);
    return 1;
  }
  
  std::cout << "Direct KV client server listening on Unix socket: " << socket_path << std::endl;
  
  while (true) {
    // Accept client connections
    int client_socket = accept(listen_socket, nullptr, nullptr);
    if (client_socket == -1) {
        std::cerr << "Failed to accept connection" << std::endl;
        continue;
    }
    
    // Handle each client in a separate thread
    std::thread client_thread(handle_connection, client_socket, db_type, db_address);
    client_thread.detach();
  }
  
  close(listen_socket);
  unlink(socket_path.c_str());
  return 0;
}
