#pragma once

#include <vector>
#include <variant>

#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/asio/local/stream_protocol.hpp>

#include "../rocksdb_server/message.hpp"
#include "kv_client.hpp"

using boost::asio::ip::tcp;
using boost::asio::local::stream_protocol;

class rocksdb_client : public kv_client
{
public:
  explicit rocksdb_client(const std::string& addr)
      : m_io_context()
      , m_socket_variant(create_socket_and_connect(addr))
  {
  }

  auto get(std::string_view key) -> std::optional<std::string> override
  {
    query_message query;
    query.set_command("get");
    query.set_key(key);
    query.set_is_valid(/*is_valid*/true);

    response_message response = execute(query);
    if (response.op_is_successful()) {
      return std::move(response.get_data());
    }
    return std::nullopt;
  }

  // To suppress bugprone-easily-swappable-parameters warning from clang-tidy
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
  auto put(std::string_view key, std::string_view value) -> bool override
  {
    query_message query;
    query.set_command("put");
    query.set_key(key);
    query.set_value(value);
    query.set_is_valid(/*is_valid*/true);

    response_message response = execute(query);
    return response.op_is_successful();
  }

  auto del(std::string_view key) -> bool override
  {
    query_message query;
    query.set_command("del");
    query.set_key(key);
    query.set_is_valid(/*is_valid*/true);

    response_message response = execute(query);
    return response.op_is_successful();
  }

  auto getm(std::string_view key_prefix) -> std::vector<std::string> override
  {
    query_message query;
    query.set_command("getm");
    query.set_key(key_prefix);
    query.set_is_valid(/*is_valid*/true);

    response_message response = execute(query);
    if (response.op_is_successful()) {
      return parse_getm_response(std::move(response.get_data()));
    }
    
    // Return empty vector if operation failed
    return {};
  }

  auto get_prefix_kv_pairs(std::string_view key_prefix) -> std::vector<std::pair<std::string, std::string>> override
  {
    query_message query;
    query.set_command("get_prefix_kv_pairs"); // New command
    query.set_key(key_prefix);
    query.set_is_valid(/*is_valid*/true);

    response_message response = execute(query);
    if (response.op_is_successful()) {
      return parse_get_prefix_kv_pairs_response(std::move(response.get_data()));
    }
    
    return {}; // Empty vector if operation failed
  }

  auto putm(const std::vector<std::pair<std::string, std::string>>& key_value_pairs) -> std::vector<bool> override {
    if (key_value_pairs.empty()) return {};
    
    query_message query;
    query.set_command("putm");
    query.set_key("");
    query.set_is_valid(true);
    
    // Serialize all key-value pairs efficiently
    std::string serialized_data = serialize_key_value_pairs(key_value_pairs);
    query.set_value(serialized_data);
    response_message response = execute(query);
    
    if (response.op_is_successful()) {
      return deserialize_bulk_results(std::move(response.get_data()), key_value_pairs.size());
    } else {
      // All failed
      std::vector<bool> results(key_value_pairs.size(), false);
      return results;
    }
  }

private:
  boost::asio::io_context m_io_context;
  std::variant<tcp::socket, stream_protocol::socket> m_socket_variant;

  auto create_socket_and_connect(const std::string& addr) -> std::variant<tcp::socket, stream_protocol::socket>
  {
    // Check if address starts with "unix://" or is a file path
    if (addr.starts_with("unix://") || addr.starts_with("/") || addr.find(':') == std::string::npos) {
      // Unix socket connection
      std::string socket_path = addr;
      if (socket_path.starts_with("unix://")) {
        socket_path = socket_path.substr(7); // Remove "unix://" prefix
      }
      
      stream_protocol::socket unix_socket(m_io_context);
      unix_socket.connect(stream_protocol::endpoint(socket_path));
      return std::move(unix_socket);
    } else {
      // TCP connection
      std::vector<std::string> host_port_splits;
      boost::split(host_port_splits, addr, boost::is_any_of(":"));
      assert(host_port_splits.size() == 2 && "DB server address must be in <host>:<port> format!");

      tcp::resolver resolver(m_io_context);
      tcp::resolver::results_type endpoints = resolver.resolve(host_port_splits[0], host_port_splits[1]);
      
      tcp::socket tcp_socket(m_io_context);
      boost::asio::connect(tcp_socket, endpoints);
      return std::move(tcp_socket);
    }
  }

  auto execute(query_message query) -> response_message
  {
    std::string raw_query = query.serialize();
    // Prepend message size to query
    int message_size = static_cast<int>(raw_query.size());
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    raw_query.insert(0, reinterpret_cast<const char*>(&message_size), sizeof(int));

    // Send query
    std::visit([&raw_query](auto& socket) {
      boost::asio::write(socket, boost::asio::buffer(raw_query));
    }, m_socket_variant);

    // Receive response size
    int response_size = 0;
    std::visit([&response_size](auto& socket) {
      boost::asio::read(socket, boost::asio::buffer(&response_size, sizeof(int)));
    }, m_socket_variant);

    // Receive response
    std::vector<char> response_buffer(static_cast<size_t>(response_size));
    std::visit([&response_buffer](auto& socket) {
      boost::asio::read(socket, boost::asio::buffer(response_buffer));
    }, m_socket_variant);

    std::string raw_response(response_buffer.begin(), response_buffer.end());
    return response_message::deserialize(std::move(raw_response));
  }

  // Helper method to parse the combined response
  // response format: [4 bytes: size1][data1][4 bytes: size2][data2]...[4 bytes: sizeN][dataN]
  auto parse_getm_response(std::string&& data) -> std::vector<std::string>
  {
    std::vector<std::string> values;
    
    if (data.empty()) {
      return values;
    }

    const char* read_ptr = data.data();
    const char* const end_ptr = data.data() + data.size();
    
    while (read_ptr < end_ptr) {
      // Check if we have enough bytes for size field
      if (read_ptr + sizeof(uint32_t) > end_ptr) {
        std::cerr << "Corrupted data: incomplete size field" << std::endl;
        break;
      }
      
      // Extract value size (network byte order)
      uint32_t value_size;
      std::memcpy(&value_size, read_ptr, sizeof(value_size));
      read_ptr += sizeof(uint32_t);
      
      // Check if we have enough bytes for the value data
      if (read_ptr + value_size > end_ptr) {
        std::cerr << "Corrupted data: incomplete value data, expected " 
                  << value_size << " bytes" << std::endl;
        break;
      }
      
      // Extract value directly into vector - single copy
      values.emplace_back(read_ptr, value_size);
      read_ptr += value_size;
    }
    
    return values;
  }

  auto parse_get_prefix_kv_pairs_response(std::string&& data) -> std::vector<std::pair<std::string, std::string>>
  {
    std::vector<std::pair<std::string, std::string>> pairs;
    
    if (data.empty()) {
      return pairs;
    }
    
    const char* read_ptr = data.data();
    const char* const end_ptr = data.data() + data.size();
    
    while (read_ptr < end_ptr) {
      // Read key size
      if (read_ptr + sizeof(uint32_t) > end_ptr) {
        std::cerr << "Corrupted data: incomplete key size field" << std::endl;
        break;
      }
      
      uint32_t key_size;
      std::memcpy(&key_size, read_ptr, sizeof(key_size));
      read_ptr += sizeof(uint32_t);
      
      // Read key data
      if (read_ptr + key_size > end_ptr) {
        std::cerr << "Corrupted data: incomplete key data" << std::endl;
        break;
      }
      
      std::string key(read_ptr, key_size);
      read_ptr += key_size;
      
      // Read value size
      if (read_ptr + sizeof(uint32_t) > end_ptr) {
        std::cerr << "Corrupted data: incomplete value size field" << std::endl;
        break;
      }
      
      uint32_t value_size;
      std::memcpy(&value_size, read_ptr, sizeof(value_size));
      read_ptr += sizeof(uint32_t);
      
      // Read value data
      if (read_ptr + value_size > end_ptr) {
        std::cerr << "Corrupted data: incomplete value data" << std::endl;
        break;
      }
      
      std::string value(read_ptr, value_size);
      read_ptr += value_size;
      
      pairs.emplace_back(std::move(key), std::move(value));
    }
    
    return pairs;
  }

    // Helper: serialize key-value pairs for network transmission
  auto serialize_key_value_pairs(const std::vector<std::pair<std::string, std::string>>& pairs) -> std::string {
    std::string result;
    
    // Calculate size to avoid reallocations
    size_t total_size = sizeof(uint32_t); // count
    for (const auto& [key, value] : pairs) {
      total_size += sizeof(uint32_t) + key.size() + sizeof(uint32_t) + value.size();
    }
    result.reserve(total_size);
    
    // Write count
    uint32_t count = static_cast<uint32_t>(pairs.size());
    result.append(reinterpret_cast<const char*>(&count), sizeof(count));
    
    // Write pairs
    for (const auto& [key, value] : pairs) {
      uint32_t key_len = static_cast<uint32_t>(key.size());
      uint32_t value_len = static_cast<uint32_t>(value.size());
      
      result.append(reinterpret_cast<const char*>(&key_len), sizeof(key_len));
      result.append(key);
      result.append(reinterpret_cast<const char*>(&value_len), sizeof(value_len));
      result.append(value);
    }
    
    return result;
  }
  
  // Helper: deserialize results from server
  auto deserialize_bulk_results(const std::string& data, size_t expected_count) -> std::vector<bool> {
    std::vector<bool> results;
    results.reserve(expected_count);
    
    const char* ptr = data.data();
    const char* end = data.data() + data.size();
    
    while (ptr < end && results.size() < expected_count) {
      bool success = (*ptr != 0);
      results.push_back(success);
      ptr++;
    }
    
    // Fill remaining with false if needed
    while (results.size() < expected_count) {
      results.push_back(false);
    }
    
    return results;
  }
};