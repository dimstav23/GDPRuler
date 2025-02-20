#pragma once

#include <vector>
#include <string>
#include <span>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <arpa/inet.h>

constexpr int s2ns = 1000000000;
constexpr int s2ms = 1000;
constexpr int ns_precision = 9;

constexpr int max_msg_size = 4096;
constexpr size_t msg_header_size = sizeof(uint32_t);

// controller response codes
constexpr std::string GET_FAILED       = "0";
constexpr std::string PUT_SUCCESS      = "1";
constexpr std::string PUT_FAILED       = "2";
constexpr std::string DELETE_SUCCESS   = "3";
constexpr std::string DELETE_FAILED    = "4";
constexpr std::string GETM_FAILED      = "5";
constexpr std::string PUTM_SUCCESS     = "6";
constexpr std::string PUTM_FAILED      = "7";
constexpr std::string GET_LOGS_FAILED  = "8";
constexpr std::string INVALID_COMMAND  = "9";
constexpr std::string UNKNOWN_ERROR    = "10";

/* Parse the value corresponding to given option. Return empty string if not found. */
auto inline get_command_line_argument(const auto& args, const std::string& option) -> std::string
{
  size_t option_index = 0;
  size_t args_size = static_cast<uint>(args.size());
  for (; option_index < args_size; option_index++) {
    if (args[option_index] == option) {
      break;
    }
  }
  if (option_index + 1 < args_size) {
    return args[option_index + 1];
  }
  return {};
}

// Function to safely close a socket
auto inline safe_close_socket(int socket) -> void {
  int result = close(socket);
  if (result != 0) {
    std::cerr << "Error closing client socket with fd:" << socket << std::endl;
  }
}

// Function to safely send a specified number of bytes to the socket
// Optimized version using sendmsg() with iovec and MSG_NOSIGNAL
auto inline safe_sock_send(int socket, void* buffer, size_t size) -> ssize_t {
  struct iovec iov[2];
  struct msghdr msg = {};

  uint32_t msg_size = htonl(size);

  // Set up iov for the message header with the size
  iov[0].iov_base = &msg_size;
  iov[0].iov_len = msg_header_size;

  // Set up iov for the actual message
  iov[1].iov_base = buffer;
  iov[1].iov_len = size;

  msg.msg_iov = iov;
  msg.msg_iovlen = 2;

  return sendmsg(socket, &msg, MSG_NOSIGNAL);
}

// Function to safely receive a specified number of bytes from the socket
// Optimized version using recvmsg() with iovec and MSG_WAITALL
auto inline safe_sock_receive(int socket, void* buffer, size_t size) -> ssize_t {
  struct iovec iov;
  struct msghdr msg = {};

  uint32_t msg_size;

  ssize_t bytes_received = recv(socket, &msg_size, sizeof(msg_size), MSG_WAITALL);
  if (bytes_received < 0) {
    std::cerr << "Failed to read the message size or the connection is closed." << std::endl;
    return bytes_received;
  }

  msg_size = ntohl(msg_size);

  if (msg_size > max_msg_size) {
    std::cerr << "Incoming message too large!" << std::endl;
    return -1;
  }

  iov.iov_base = buffer;
  iov.iov_len = msg_size;
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;

  return recvmsg(socket, &msg, MSG_WAITALL);
}