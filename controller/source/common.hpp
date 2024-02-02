#pragma once

#include <vector>
#include <string>
#include <span>
#include <sys/socket.h>

constexpr int s2ns = 1000000000;
constexpr int s2ms = 1000;
constexpr int ns_precision = 9;

constexpr int max_msg_size = 8192;

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