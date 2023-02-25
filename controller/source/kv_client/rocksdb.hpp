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
    if (response.get_is_success()) {
      // std::cout << "GET operation succeeded! Key: " << key << ", Value: " << response.response << std::endl;
    } else {
      std::cout << "GET operation failed" << std::endl;
    }
    return response.get_data();
  }

  auto put(const std::string& key, const std::string& value) -> bool override
  {
    query_message query;
    query.set_command("put");
    query.set_key(key);
    query.set_value(value);
    query.set_is_valid(/*is_valid*/true);

    response_message response = execute(query);
    if (response.get_is_success()) {
      // std::cout << "PUT operation succeeded! Key: " << key << ", Value: " << value << std::endl;
    } else {
      std::cout << "PUT operation failed" << std::endl;
    }
    return response.get_is_success();
  }

  auto del(const std::string& key) -> bool override
  {
    query_message query;
    query.set_command("del");
    query.set_key(key);
    query.set_is_valid(/*is_valid*/true);

    response_message response = execute(query);
    if (response.get_is_success()) {
      // std::cout << "DELETE operation succeeded! Key: " << key << std::endl;
    } else {
      std::cout << "DELETE operation failed" << std::endl;
    }
    return response.get_is_success();
  }

private:
  boost::asio::io_context m_io_context;
  boost::asio::ip::tcp::socket m_socket;

  auto execute(query_message query) -> response_message
  {
    std::string raw_query = query.serialize();
    boost::asio::write(m_socket, boost::asio::buffer(raw_query));

    // Receive a response from the server
    boost::asio::streambuf receive_buffer;
    boost::asio::read_until(m_socket, receive_buffer, "\n");
    std::string raw_response(boost::asio::buffers_begin(receive_buffer.data()), boost::asio::buffers_end(receive_buffer.data()) - 1);
    return response_message::deserialize(raw_response);
  }
};