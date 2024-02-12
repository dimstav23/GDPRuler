#pragma once

#include <vector>

#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>

#include "../rocksdb_server/message.hpp"
#include "kv_client.hpp"

class rocksdb_client : public kv_client
{
public:
  explicit rocksdb_client(const std::string& addr)
      : m_socket(m_io_context)
  {
    std::vector<std::string> host_port_splits;
    boost::split(host_port_splits, addr, boost::is_any_of(":"));
    assert(host_port_splits.size() == 2 && "DB server address must be in <host>:<port> format!");

    // Resolve the host and port to an endpoint
    using boost::asio::ip::tcp;
    tcp::resolver resolver(m_io_context);
    tcp::resolver::results_type endpoints = resolver.resolve(host_port_splits[0], host_port_splits[1]);

    boost::asio::connect(m_socket, endpoints);
  }

  auto get(const std::string& key) -> std::optional<std::string> override
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
  auto put(const std::string& key, const std::string& value) -> bool override
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

  auto del(const std::string& key) -> bool override
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

private:
  boost::asio::io_context m_io_context;
  boost::asio::ip::tcp::socket m_socket;

  auto execute(query_message query) -> response_message
  {
    std::string raw_query = query.serialize();

    // Prepend message size to query
    int message_size = static_cast<int>(raw_query.size());
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    raw_query.insert(0, reinterpret_cast<const char*>(&message_size), sizeof(int));

    // Send query
    boost::asio::write(m_socket, boost::asio::buffer(raw_query));

    // Receive response size
    int response_size = 0;
    boost::asio::read(m_socket, boost::asio::buffer(&response_size, sizeof(int)));

    // Receive response
    std::vector<char> response_buffer(static_cast<size_t>(response_size));
    boost::asio::read(m_socket, boost::asio::buffer(response_buffer));
    std::string raw_response(response_buffer.begin(), response_buffer.end());
    return response_message::deserialize(raw_response);
  }
};