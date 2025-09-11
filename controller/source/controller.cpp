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
#include "logging/monitor.hpp"
#include "gdpr_regulator.hpp"
#include "global_gdpr_metadata_cache.hpp"

using controller::default_policy;
using controller::cipher_engine;
using controller::query;
using controller::query_rewriter;
using controller::gdpr_filter;
using controller::logger;
using controller::gdpr_monitor;
using controller::gdpr_regulator;

#ifdef METADATA_CACHE
// Define the cache size (in keys)
static constexpr size_t GDPR_METADATA_CACHE_SIZE = (1 << 20); // 1048576 cached keys / 16384 per shard
static auto& cache = controller::GdprMetadataCache::get_instance(GDPR_METADATA_CACHE_SIZE);
#endif

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

// Possible cases:
// 1. Key does not exist in cache or DB            (cache miss + DB miss) -> {monitor, ""}, is_valid = false
// 2. Key is in cache, but validation fails        (cache hit, no DB lookup) -> {monitor, cached_metadata}, is_valid = false
// 3. Key is in cache, validation succeeds         (cache hit, no DB lookup) -> {monitor, cached_metadata}, is_valid = true
// 4. Key is not in cache, found in DB, invalid    (cache miss + DB hit) -> {monitor, complete_value}, is_valid = false
// 5. Key is not in cache, found in DB, valid      (cache miss + DB hit) -> {monitor, complete_value}, is_valid = true
auto filter_and_monitor(const std::unique_ptr<kv_client>& client,
  const query& query_args,
  const default_policy& def_policy,
  bool& is_valid,
  bool& cache_hit)
  -> std::tuple<gdpr_monitor, std::optional<std::string>>
{
  #ifdef METADATA_CACHE
  // Check if the key is in the cache
  std::optional<std::string> metadata_or_value;
  auto cached_metadata = cache.cache_get(query_args.key());
  if (cached_metadata) {
    metadata_or_value = std::make_optional(*cached_metadata);
    cache_hit = true;
  }
  else {
    // Cache miss - fetch from DB
    metadata_or_value = client->gdpr_get(query_args.key());
  }
  #else
  // No cache - always fetch from DB
  auto metadata_or_value = client->gdpr_get(query_args.key());
  #endif

  if (!metadata_or_value) {
    // if the key does not exist in cache or DB
    is_valid = true;
    auto monitor = gdpr_monitor(query_args, def_policy);
    return {std::move(monitor), std::nullopt};
  }

  // if the key exists in cache or DB
  // Create a filter and monitor object and validate the query
  gdpr_filter filter(metadata_or_value);
  is_valid = filter.validate(query_args, def_policy);
  auto monitor = gdpr_monitor(filter, query_args, def_policy);
  return {std::move(monitor), std::move(metadata_or_value)};
}

/* Get the value associated with a key */
inline auto handle_get(const std::unique_ptr<kv_client>& client,
                const query& query_args,
                const default_policy& def_policy) -> std::string
{
  // Always fetch from database as even in cache hit, we need to fetch the value
  auto res = client->gdpr_get(query_args.key());
  gdpr_filter filter(res);
  bool is_valid = filter.validate(query_args, def_policy);

  // Create monitor and log
  auto monitor = gdpr_monitor(filter, query_args, def_policy);
  monitor.monitor_query(is_valid);

  if (is_valid && res) {
    #ifdef DEBUG
    std::cout << "Get query: " << query_args.key() << " with value: " << hex_dump(res.value()) << std::endl;
    #endif
    return controller::remove_gdpr_metadata(std::move(res.value()));
  }

  return GET_FAILED;
}

/* Insert a KV pair or update an existing value -- GDPR metadata is preserved */
inline auto handle_put(const std::unique_ptr<kv_client>& client,
                const query& query_args,
                const default_policy& def_policy) -> std::string
{
  bool query_is_valid = false;
  bool cache_hit = false;
  auto [monitor, metadata_or_value] = filter_and_monitor(client, query_args, def_policy, query_is_valid, cache_hit);

  // Early exit for invalid operations
  if (!query_is_valid) {
    monitor.monitor_query(query_is_valid);
    return PUT_FAILED;
  }

  // Construct new value
  std::string new_value;
  if (!metadata_or_value) {
    // New key insertion
    query_rewriter rewriter(query_args, def_policy, query_args.value());
    new_value = std::move(rewriter).new_value();
  } else {
    // Update existing key (works for both cache hit and DB hit as it needs only the metadata)
    query_rewriter rewriter(metadata_or_value.value(), query_args.value());
    new_value = std::move(rewriter).new_value();
  }

  // Monitor and execute the put operation
  monitor.monitor_query(query_is_valid, new_value);
  #ifdef DEBUG
  std::cout << "Put query: " << query_args.key() << " with value: " << hex_dump(new_value) << std::endl;
  #endif
  auto ret_val = client->gdpr_put(query_args.key(), new_value);
  
  if (ret_val) {
    #ifdef METADATA_CACHE
    // Only update cache if it wasn't a cache hit
    if (!cache_hit) {
      std::string metadata = controller::preserve_only_gdpr_metadata(std::move(new_value));
      #ifdef DEBUG
      std::cout << "Caching metadata for key: " << query_args.key() << " in format:" << hex_dump(metadata) << std::endl;
      #endif
      cache.cache_put(query_args.key(), std::move(metadata));
    }
    #endif
    return PUT_SUCCESS;
  }

  return PUT_FAILED;
}

/* Delete a KV pair */
inline auto handle_delete(const std::unique_ptr<kv_client>& client,
                  const query& query_args,
                  const default_policy& def_policy) -> std::string
{
  bool query_is_valid = false;
  bool cache_hit = false;
  auto [monitor, metadata_or_value] = filter_and_monitor(client, query_args, def_policy, query_is_valid, cache_hit);

  if (!metadata_or_value) {
    // Key does not exist in cache or DB - try to delete non-existing key
    monitor.monitor_query(false);
    return DELETE_FAILED;
  }
  // Log the operation (if needed)
  monitor.monitor_query(query_is_valid);

  if (query_is_valid) {
    auto ret_val = client->gdpr_del(query_args.key());
    if (ret_val) {
      #ifdef METADATA_CACHE
      // Remove from cache
      cache.cache_remove(query_args.key());
      #endif
      return DELETE_SUCCESS;
    }
  }

  return DELETE_FAILED;
}

inline auto handle_get_metadata(const std::unique_ptr<kv_client> &client,
                const query &query_args,
                const default_policy &def_policy) -> std::string
{
  auto res = client->gdpr_getm(query_args.key());
  gdpr_filter filter(res);

  // Check if the retrieved key requires logging
  auto monitor = gdpr_monitor(filter, query_args, def_policy);
  bool is_valid = filter.validate(query_args, def_policy);
  // Perform the logging of the (in)valid operation -- if needed
  monitor.monitor_query(is_valid);
  if (is_valid) {
    // if the key exists and complies with the gdpr rules
    // then return the GDPR metadata of the key
    return controller::preserve_only_gdpr_metadata(std::move(res.value()));
  }

  return GETM_FAILED; // GETM_FAILED: Invalid key or does not comply with GDPR rules
}

inline auto handle_put_metadata(const std::unique_ptr<kv_client> &client,
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
  gdpr_filter filter(res);
  if ((is_valid = filter.validate(query_args, def_policy))) {
    // Check if the retrieved value requires logging
    // the query args do not need to be checked since they cannot update the
    // gpdr metadata of the value -- only putm operations can
    auto monitor = gdpr_monitor(filter, query_args, def_policy);
    // update the current value with the new one without modifying any metadata
    query_rewriter rewriter(query_args, res.value(), std::nullopt);
    std::string new_value = std::move(rewriter).new_value();

    // Perform the logging of the valid operation -- if needed
    monitor.monitor_query(is_valid, new_value);
    auto ret_val = client->gdpr_putm(query_args.key(), new_value);
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

/* Insert a KV pair or update an existing value -- GDPR metadata can be altered */
inline auto handle_put_combined(const std::unique_ptr<kv_client>& client,
                const query& query_args,
                const default_policy& def_policy) -> std::string
{
  bool query_is_valid = false;
  bool cache_hit = false;
  auto [monitor, metadata_or_value] = filter_and_monitor(client, query_args, def_policy, query_is_valid, cache_hit);

  // Early exit for invalid operations
  if (!query_is_valid) {
    monitor.monitor_query(query_is_valid);
    return PUTC_FAILED;
  }

  // Construct new value with the new/updated metadata
  std::string new_value;
  if (!metadata_or_value) {
    // New key insertion
    query_rewriter rewriter(query_args, def_policy, query_args.value());
    new_value = std::move(rewriter).new_value();
  } else {
    // Update the metadata of an existing key
    query_rewriter rewriter(query_args, metadata_or_value.value(), query_args.value());
    new_value = std::move(rewriter).new_value();
  }

  // Monitor and execute the put operation
  monitor.monitor_query(query_is_valid, new_value);
  #ifdef DEBUG
  std::cout << "Putc query: " << query_args.key() << " with value: " << hex_dump(new_value) << std::endl;
  #endif
  auto ret_val = client->gdpr_put(query_args.key(), new_value);
  
  if (ret_val) {
    #ifdef METADATA_CACHE
    // Update cache
    std::string metadata = controller::preserve_only_gdpr_metadata(std::move(new_value));
    #ifdef DEBUG
    std::cout << "Caching metadata for key: " << query_args.key() << " in format:" << hex_dump(metadata) << std::endl;
    #endif
    cache.cache_put(query_args.key(), std::move(metadata));
    #endif
    return PUTC_SUCCESS;
  }

  return PUTC_FAILED;
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

#ifdef INTERNAL_TIMING
auto handle_exit(bool is_benchmark_thread, 
                std::chrono::duration<double> local_processing_time,
                std::chrono::duration<double> local_frontend_connection_time) -> std::string {
  if (is_benchmark_thread) {
    int remaining = g_active_benchmark_threads.fetch_sub(1) - 1;
    
    if (remaining == 0) {
      // Last thread - generate full timing report
      return generate_timing_response(local_processing_time, local_frontend_connection_time);
    } else {
      // Not the last thread - just add to globals
      double current_processing = g_total_processing_time_seconds.load();
      while (!g_total_processing_time_seconds.compare_exchange_weak(current_processing, current_processing + local_processing_time.count())) {}
      
      double current_connection = g_total_connection_time_seconds.load();
      while (!g_total_connection_time_seconds.compare_exchange_weak(current_connection, current_connection + local_frontend_connection_time.count())) {}
      
      return "Client exiting";
    }
  } else {
    return "Loading phase completed";
  }
}
#endif

auto handle_connection
(int socket, const std::string& db_type, const std::string& db_address) -> void
{
  #ifdef INTERNAL_TIMING
  bool is_benchmark_thread = start_benchmark_timing();
  std::chrono::duration<double> local_processing_time{0};
  std::chrono::duration<double> local_frontend_connection_time{0};
  #endif

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

  while (true) {
    #ifdef INTERNAL_TIMING
    auto frontend_connection_rec_start = std::chrono::steady_clock::now();
    #endif

    // Read the message size from the socket
    ssize_t bytes_read = safe_sock_receive(socket, buffer);
    if (bytes_read <= 0) {
      std::cerr << "Failed to read the message or the connection is closed." << std::endl;
      break;
    }
    
    #ifdef INTERNAL_TIMING
    auto frontend_connection_rec_end = std::chrono::steady_clock::now();
    auto processing_start = std::chrono::steady_clock::now();
    #endif

    // Set the termination character for the string
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    (static_cast<char*>(buffer))[bytes_read] = '\0';
    
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    query query_args(static_cast<char*>(buffer));
    std::string response;

    if (query_args.cmd() == "exit") [[unlikely]] {
      #ifdef INTERNAL_TIMING
      response = handle_exit(is_benchmark_thread, local_processing_time, local_frontend_connection_time);
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
      else if (query_args.cmd() == "putc") {
        response = handle_put_combined(client, query_args, def_policy);
      }
      else if (query_args.cmd() == "getlogs") {
        // current client resembles the regulator
        response = handle_get_logs(query_args, def_policy);
      }
      else {
        response = INVALID_COMMAND;
      }
    }

    #ifdef INTERNAL_TIMING
    auto processing_end = std::chrono::steady_clock::now();
    auto frontend_connection_send_start = std::chrono::steady_clock::now();
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

    #ifdef INTERNAL_TIMING
    auto frontend_connection_send_end = std::chrono::steady_clock::now();
    // Accumulate timing for this request
    accumulate_timing(is_benchmark_thread, local_processing_time, local_frontend_connection_time,
                     processing_start, processing_end, 
                     frontend_connection_rec_start, frontend_connection_rec_end,
                     frontend_connection_send_start, frontend_connection_send_end);
    #endif
  }

  #ifdef CACHE_STATS
  std::cout << "Cache size: " << cache.total_size() << "\n";
  std::cout << "Cache hits: " << cache.cache_hits() << "\n";
  std::cout << "Cache misses: " << cache.cache_misses() << "\n";
  std::cout << "Cache hit rate: " << cache.cache_hit_rate() * 100 << "%\n";
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
  if (db_address.empty()) {
    if (db_type == "redis") {
      db_address = "unix:///tmp/redis.sock"; // Default Unix socket path for Redis
    }
    else if (db_type == "rocksdb") {
      db_address = "/tmp/rocksdb.sock"; // Default Unix socket path for RocksDB
    }
  }
  
  // set the log path based on the input parameter
  const std::string log_path = get_command_line_argument(args, "--logpath");
  logger::get_instance()->init_gdpr_logger(log_path);

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
