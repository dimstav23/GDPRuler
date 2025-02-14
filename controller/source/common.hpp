#pragma once

#include <vector>
#include <string>
#include <span>
#include <unistd.h>
#include <sys/socket.h>

constexpr int s2ns = 1000000000;
constexpr int s2ms = 1000;
constexpr int ns_precision = 9;

constexpr int max_msg_size = 8192;

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

// Function to safely receive a specified number of bytes from the socket
auto inline safe_sock_receive(int socket, void* buffer, size_t size) -> ssize_t {
  char* ptr = static_cast<char*>(buffer);
  size_t total_bytes_received = 0;

  while (total_bytes_received < size) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    ssize_t bytes_received = recv(socket, ptr + total_bytes_received, size - total_bytes_received, 0);
    if (bytes_received <= 0) {
      // Failed to receive bytes or connection closed
      return bytes_received;
    }
    total_bytes_received += static_cast<size_t>(bytes_received);
  }

  return static_cast<ssize_t>(total_bytes_received);
}

// Function to safely send a specified number of bytes to the socket
auto inline safe_sock_send(int socket, const void* buffer, size_t size) -> ssize_t {
  const char* ptr = static_cast<const char*>(buffer);
  size_t total_bytes_sent = 0;

  while (total_bytes_sent < size) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    ssize_t bytes_sent = send(socket, ptr + total_bytes_sent, size - total_bytes_sent, 0);
    if (bytes_sent <= 0) {
      // Failed to send bytes or connection closed
      return bytes_sent;
    }
    total_bytes_sent += static_cast<size_t>(bytes_sent);
  }

  return static_cast<ssize_t>(total_bytes_sent);
}