#include <iostream>
#include <string>
#include <span>
#include <vector>
#include <memory>
#include <boost/asio.hpp>

#include "rocksdb_proxy.hpp"

using boost::asio::ip::tcp;

class session : public std::enable_shared_from_this<session> {
public:
  session(tcp::socket socket, std::shared_ptr<rocksdb_proxy> rocksdb_proxy)
      : m_socket(std::move(socket))
      , m_rocksdb_proxy(std::move(rocksdb_proxy))
  {
  }

  void start() {
    handle_read();
  }

private:
  void handle_read() {
    auto self(shared_from_this());
    boost::asio::async_read_until(m_socket, m_buffer, '\n',
      [this, self](boost::system::error_code error_code, std::size_t length) {
      if (!error_code) {
        // put buffered data into string excluding the last new line character
        auto buffer_begin = boost::asio::buffers_begin(m_buffer.data());
        std::string raw_query(buffer_begin, buffer_begin + static_cast<int>(length) - 1);
        m_buffer.consume(length);
        query_message query = query_message::deserialize(raw_query);
        response_message response = m_rocksdb_proxy->execute(query);
        std::string raw_response = response.serialize();
        boost::asio::write(m_socket, boost::asio::buffer(raw_response));
        handle_read();
      }
      else if (error_code == boost::asio::error::eof) {
        std::cout << "Client is finished with the queries. Closing the session..." << std::endl;
      }
      else {
        std::cout << "Error while reading tcp message: " << error_code << std::endl;
      }
    });
  }

  tcp::socket m_socket;
  std::shared_ptr<rocksdb_proxy> m_rocksdb_proxy;
  boost::asio::streambuf m_buffer;
};

class rocksdb_server {
public:
  rocksdb_server(boost::asio::io_service& io_service,
                 uint16_t port,
                 const std::string& db_path)
      : m_acceptor(io_service, tcp::endpoint(tcp::v4(), port))
      , m_socket(io_service)
      , m_rocksdb_proxy(make_shared<rocksdb_proxy>(db_path))
  {
    std::cout << "Starting server on port: " << port << std::endl;
    do_accept();
  }

private:
  void do_accept() {
    std::cout << "Server is waiting to accept a new request!" << std::endl;
    m_acceptor.async_accept(m_socket, [this](boost::system::error_code error_code) {
      if (!error_code) {
        std::make_shared<session>(std::move(m_socket), m_rocksdb_proxy)->start();
      }
      do_accept();
    });
  }

  tcp::acceptor m_acceptor;
  tcp::socket m_socket;
  std::shared_ptr<rocksdb_proxy> m_rocksdb_proxy;
};

auto main(int argc, char* argv[]) -> int {
  auto args = std::span(argv, static_cast<size_t>(argc));

  try {
    assert(argc == 3 && "Usage: ./rocksdb_server <port> <db_path>");
    boost::asio::io_service io_service;
    rocksdb_server rocksdb_server(io_service, static_cast<uint16_t>(std::stoul(args[1])), args[2]);
    io_service.run();
  } catch (std::exception& e) {
    std::cerr << "Exception in Rocksdb server: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
