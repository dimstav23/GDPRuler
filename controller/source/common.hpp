#pragma once

#include <vector>
#include <string>
#include <string_view>
#include <sstream>
#include <iomanip>
#include <span>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <arpa/inet.h>

constexpr int s2ns = 1000000000;
constexpr int s2ms = 1000;
constexpr int ns_precision = 9;

constexpr int max_msg_size = 8192;
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
constexpr std::string PUTC_SUCCESS     = "8";
constexpr std::string PUTC_FAILED      = "9";
constexpr std::string GET_LOGS_FAILED  = "10";
constexpr std::string INVALID_COMMAND  = "11";
constexpr std::string UNKNOWN_ERROR    = "12";

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
auto inline safe_sock_receive(int socket, void* buffer) -> ssize_t {
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

/* Debug printing functions for binary data */
/**
 * Creates a hex dump of binary data with optional ASCII representation
 * 
 * @param data The binary data to dump (string or string_view)
 * @param show_ascii Whether to show ASCII representation alongside hex
 * @param bytes_per_line Number of bytes to display per line (default: 16)
 * @return String containing the formatted hex dump
 */
auto hex_dump(std::string_view data, bool show_ascii = true, size_t bytes_per_line = 16) -> std::string {
  std::stringstream hex;
  
  hex << "=== HEX DUMP ===\n";
  hex << "Size: " << data.size() << " bytes\n";
  
  for (size_t i = 0; i < data.size(); ++i) {
    // Print offset at start of each line
    if (i % bytes_per_line == 0) {
      hex << std::hex << std::setw(4) << std::setfill('0') << i << ": ";
    }
    
    // Print hex value
    hex << std::hex << std::setw(2) << std::setfill('0') 
        << static_cast<int>(static_cast<uint8_t>(data[i])) << " ";
    
    // End of line or end of data
    if (i % bytes_per_line == bytes_per_line - 1 || i == data.size() - 1) {
      // Add padding if not a full line
      if (i % bytes_per_line != bytes_per_line - 1) {
        size_t padding = (bytes_per_line - 1 - (i % bytes_per_line)) * 3;
        hex << std::string(padding, ' ');
      }
      
      // Add ASCII representation if requested
      if (show_ascii) {
        hex << " |";
        size_t line_start = i - (i % bytes_per_line);
        size_t line_end = std::min(line_start + bytes_per_line, data.size());
        
        for (size_t j = line_start; j < line_end; ++j) {
          char c = data[j];
          hex << (std::isprint(c) ? c : '.');
        }
        hex << "|";
      }
      
      hex << "\n";
    }
  }
  
  return hex.str();
}

// Overload for std::string (automatically converts to string_view)
auto hex_dump(const std::string& data, bool show_ascii = true, size_t bytes_per_line = 16) -> std::string{
  return hex_dump(std::string_view(data), show_ascii, bytes_per_line);
}


/* Helper functions for internal timing measurements and reporting */
#ifdef INTERNAL_TIMING
#include <chrono>
#include <atomic>
#include <mutex>
#include <iomanip>

// Simple timing variables
static std::atomic<bool> g_skip_first_connection{true};  // Skip loading phase
static std::atomic<bool> g_timing_started{false};       // Track if timing started
static std::atomic<int> g_active_benchmark_threads{0};  // Count active threads
static std::mutex g_timing_mutex;
static std::chrono::steady_clock::time_point g_start_time;
static std::chrono::steady_clock::time_point g_end_time;
static std::atomic<std::chrono::duration<double>::rep> g_total_processing_time_seconds{0.0};
static std::atomic<std::chrono::duration<double>::rep> g_total_connection_time_seconds{0.0};

// Timing helper functions
inline auto start_benchmark_timing() -> bool {
  static bool first_call = true;
  if (g_skip_first_connection.exchange(false)) {
    // Loading connection
    return false;
  } else {
    // Benchmark connection
    g_active_benchmark_threads.fetch_add(1);
    if (!g_timing_started.exchange(true)) {
      std::lock_guard<std::mutex> lock(g_timing_mutex);
      g_start_time = std::chrono::steady_clock::now();
    }
    return true;
  }
}

inline auto accumulate_timing(bool is_benchmark, 
                             std::chrono::duration<double>& local_processing, 
                             std::chrono::duration<double>& local_frontend_connection,
                             const std::chrono::steady_clock::time_point& processing_start,
                             const std::chrono::steady_clock::time_point& processing_end,
                             const std::chrono::steady_clock::time_point& frontend_connection_rec_start,
                             const std::chrono::steady_clock::time_point& frontend_connection_rec_end,
                             const std::chrono::steady_clock::time_point& frontend_connection_send_start,
                             const std::chrono::steady_clock::time_point& frontend_connection_send_end) -> void {
  if (is_benchmark) {
    auto processing_duration = std::chrono::duration_cast<std::chrono::duration<double>>(processing_end - processing_start);
    auto frontend_connection_rec_duration = std::chrono::duration_cast<std::chrono::duration<double>>(frontend_connection_rec_end - frontend_connection_rec_start);
    auto frontend_connection_send_duration = std::chrono::duration_cast<std::chrono::duration<double>>(frontend_connection_send_end - frontend_connection_send_start);
    auto frontend_connection_duration = frontend_connection_rec_duration + frontend_connection_send_duration;
    
    local_processing += processing_duration;
    local_frontend_connection += frontend_connection_duration;
  }
}

inline auto generate_timing_response(std::chrono::duration<double> local_processing,
                                    std::chrono::duration<double> local_connection) -> std::string {
  // Add this thread's times to global totals
  double current_processing = g_total_processing_time_seconds.load();
  while (!g_total_processing_time_seconds.compare_exchange_weak(current_processing, current_processing + local_processing.count())) {}
  
  double current_connection = g_total_connection_time_seconds.load();
  while (!g_total_connection_time_seconds.compare_exchange_weak(current_connection, current_connection + local_connection.count())) {}
  
  // Calculate final metrics
  std::lock_guard<std::mutex> lock(g_timing_mutex);
  g_end_time = std::chrono::steady_clock::now();
  auto wall_duration = std::chrono::duration_cast<std::chrono::duration<double>>(g_end_time - g_start_time);
  double total_processing = g_total_processing_time_seconds.load();
  double total_connection = g_total_connection_time_seconds.load();
  double total_work = total_processing + total_connection;
  
  return "Wall: " + std::to_string(wall_duration.count()) + 
         "s, Processing: " + std::to_string(total_processing) + 
         "s (" + std::to_string((total_processing/wall_duration.count())*100.0) + "%), " +
         "Connection: " + std::to_string(total_connection) + 
         "s (" + std::to_string((total_connection/wall_duration.count())*100.0) + "%), " +
         "Utilization: " + std::to_string((total_work/wall_duration.count())*100.0) + "%";
}
#endif