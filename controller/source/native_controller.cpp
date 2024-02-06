#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "kv_client/factory.hpp"
#include "query.hpp"
#include "common.hpp"
// #include "argh.hpp"

using controller::query;

auto handle_get(const query &query_args, std::unique_ptr<kv_client> &client) -> std::string
{
  auto ret_val = client->gdpr_get(query_args.key());
  if (ret_val) {
    return ret_val.value();
  } 
  return "GET_FAILED: Invalid key";
}

auto handle_put(const query &query_args, std::unique_ptr<kv_client> &client) -> std::string
{
  bool success = client->gdpr_put(query_args.key(), query_args.value());
  if (success) {
    return "PUT_SUCCESS";
  } 
  return "PUT_FAILED: Failed to put value";
}

auto handle_delete(const query &query_args, std::unique_ptr<kv_client> &client) -> std::string
{
  bool success = client->gdpr_del(query_args.key());
  if (success) {
    return "DELETE_SUCCESS";
  }
  return "DELETE_FAILED: Failed to delete key";
}

auto handle_connection(int socket, const std::string& db_type, const std::string& db_address) -> void
{
  constexpr size_t header_size = sizeof(uint32_t);  // Size of the header containing the message size
  std::vector<char> buffer(max_msg_size);  // Buffer to hold the message and its size

  // create the connection with the database instance
  std::unique_ptr<kv_client> client = kv_factory::create(db_type, db_address);

  while (true) {
    // Read the message size from the socket
    ssize_t bytes_read = safe_sock_receive(socket, buffer.data(), header_size);
    if (bytes_read != header_size) {
      std::cerr << "Failed to read the message size or the connection is closed." << std::endl;
      break;
    }

    // Convert network byte order to host byte order
    uint32_t msg_size = 0;
    std::memcpy(&msg_size, buffer.data(), sizeof(uint32_t));
    msg_size = ntohl(msg_size);

    // Resize the buffer if needed (+-1 for the null termination character)
    if (msg_size + header_size > buffer.size() - 1) {
      buffer.resize(msg_size + header_size + 1);
    }

    // Read the message data from the socket
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    bytes_read = safe_sock_receive(socket, buffer.data() + header_size, msg_size);
    if (bytes_read != static_cast<ssize_t>(msg_size)) {
      std::cerr << "Failed to read the message or the connection is closed." << std::endl;
      break;
    }

    // Set the termination character for the string
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    buffer[static_cast<size_t>(bytes_read) + header_size] = '\0';

    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    const query query_args(buffer.data() + header_size);
    std::string response;

    if (query_args.cmd() == "get") {
      response = handle_get(query_args, client);
    } else if (query_args.cmd() == "put") {
      response = handle_put(query_args, client);
    } else if (query_args.cmd() == "delete") {
      response = handle_delete(query_args, client);
    } else if (query_args.cmd() == "exit") {
      std::cout << "Client exiting..." << std::endl;
      break;
    } else {
      // std::cout << "Invalid command" << std::endl;
      response = "Invalid command";
    }

    // Resize the buffer (if needed) to accommodate the response
    if (header_size + response.length() > buffer.size()) {
      buffer.resize(header_size + response.length());
    }

    // Prepare the response size header
    auto response_size = static_cast<uint32_t>(response.length());
    response_size = htonl(response_size);
    std::memcpy(buffer.data(), &response_size, header_size);

    // Copy the response data to the buffer
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    std::memcpy(buffer.data() + header_size, response.c_str(), response.length());

    // Send the response to the client
    ssize_t bytes_sent = safe_sock_send(socket, buffer.data(), header_size + response.length());
    if (bytes_sent <= 0) {
      // Failed to send the response or connection closed
      std::cerr << "Failed to send the response to the client or the connection is closed." << std::endl;
      break;
    }
  }
  // Close the client socket
  int result = close(socket);
  if (result != 0) {
    std::cerr << "Error closing client socket" << std::endl;
  }
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

  // Setup the frontend server (controller) socket
  struct sockaddr_in server_address{};
  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = inet_addr(controller_address.c_str());
  server_address.sin_port = htons(static_cast<uint16_t>(std::stoi(controller_port)));

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
    // The client socket must be independently managed by the thread now
    std::thread connection_thread(handle_connection, client_socket, db_type, db_address);
    connection_thread.detach();  // Detach the thread and let it run independently
  }

  return 0;
}
