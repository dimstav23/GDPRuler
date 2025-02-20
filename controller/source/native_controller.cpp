#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <sys/mman.h>

#include "kv_client/factory.hpp"
#include "query.hpp"
#include "common.hpp"
// #include "argh.hpp"

#ifdef DEBUG
#include <chrono>
#endif

using controller::query;

inline auto handle_get(const query &query_args, std::unique_ptr<kv_client> &client) -> std::string
{
  std::string_view key = query_args.key();
  auto ret_val = client->gdpr_get(key);
  if (ret_val) {
    return std::move(ret_val.value());
  } 
  return GET_FAILED; // GET_FAILED: Non existing key
}

inline auto handle_put(const query &query_args, std::unique_ptr<kv_client> &client) -> std::string
{
  std::string_view key = query_args.key();
  std::string_view value = query_args.value();
  bool success = client->gdpr_put(key, value);
  if (success) {
    return PUT_SUCCESS;
  } 
  return PUT_FAILED; // PUT_FAILED: Failed to put value
}

inline auto handle_delete(const query &query_args, std::unique_ptr<kv_client> &client) -> std::string
{
  std::string_view key = query_args.key();
  bool success = client->gdpr_del(key);
  if (success) {
    return DELETE_SUCCESS;
  }
  return DELETE_FAILED; // DELETE_FAILED: Failed to delete key
}

auto handle_connection(int socket, const std::string& db_type, const std::string& db_address) -> void
{
  // Allocate a large buffer using mmap to hold the message and its size
  void* buffer = mmap(nullptr, max_msg_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (buffer == MAP_FAILED) {
    std::cerr << "Failed to allocate buffer" << std::endl;
    return;
  }

  // create the connection with the database instance
  std::unique_ptr<kv_client> client = kv_factory::create(db_type, db_address);

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
