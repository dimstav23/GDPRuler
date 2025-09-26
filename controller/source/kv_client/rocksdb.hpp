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
      // std::cout << "GET operation succeeded! Key: " << key << ", Value: " << response.get_data() << std::endl;
      return response.get_data();
    }
    // std::cout << "GET operation failed" << std::endl;
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
    // std::cout << "PUT operation request Key: " << key << std::endl;
    // if (response.op_is_successful()) {
      // std::cout << "PUT operation succeeded! Key: " << key << std::endl;
    // } else {
      // std::cout << "PUT operation failed" << std::endl;
    // }
    return response.op_is_successful();
  }

  auto del(std::string_view key) -> bool override
  {
    query_message query;
    query.set_command("del");
    query.set_key(key);
    query.set_is_valid(/*is_valid*/true);

    response_message response = execute(query);
    // if (response.op_is_successful()) {
      // std::cout << "DELETE operation succeeded! Key: " << key << std::endl;
    // } else {
      // std::cout << "DELETE operation failed" << std::endl;
    // }
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
      return parse_values_response(std::move(response.get_data()));
    }
    
    // Return empty vector if operation failed
    return {};
  }

  auto putm(std::string_view key, std::string_view value) -> bool override
  {
    query_message query;
    query.set_command("putm");
    query.set_key(key);
    query.set_value(value);
    query.set_is_valid(/*is_valid*/true);

    response_message response = execute(query);
    return response.op_is_successful();
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
    return response_message::deserialize(raw_response);
  }

  // Helper method to parse the combined response
  auto parse_values_response(std::string&& data) -> std::vector<std::string>
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
};