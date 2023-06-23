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

auto handle_connection(int socket, const std::string& db_type, const std::string& db_address) -> void
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
    if (query_args.cmd() == "get") {
      client->gdpr_get(query_args.key());
    } else if (query_args.cmd() == "put") {
      client->gdpr_put(query_args.key(), query_args.value());
    } else if (query_args.cmd() == "delete") {
      client->gdpr_del(query_args.key());
    } else if (query_args.cmd() == "exit") {
      std::cout << "Client exiting..." << std::endl;
      break;
    } else {
      std::cout << "Invalid command" << std::endl;
    }

    // Send an acknowledgment (ACK) back to the client
    std::string ack_message = "ACK";
    ssize_t bytes_sent = send(socket, ack_message.c_str(), ack_message.length(), 0);
    if (bytes_sent <= 0) {
      // Failed to send ACK or connection closed
      break;
    }
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
    std::thread connection_thread(handle_connection, client_socket, db_type, db_address);
    connection_thread.detach();  // Detach the thread and let it run independently
  }

  return 0;
}
