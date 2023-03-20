#include <iostream>
#include <string>
#include <span>
#include <vector>
#include <memory>
#include <boost/asio.hpp>
#include <thread>

#include "rocksdb_proxy.hpp"

using boost::asio::ip::tcp;

// io_service is the entry point to use boost's async capabilities. It is an interface to the OS I/O services.
// It manages the threads and the event loop related to connections and handler callbacks. 
// See here for more info: https://www.boost.org/doc/libs/1_65_1/doc/html/boost_asio/overview/core/basics.html
//NOLINTNEXTLINE 
boost::asio::io_service io_service;

/**
 * session class represents a connection handler for a single client.
 * 
 * Given a socket and a rocksdb_proxy, 
 *  it listens the socket asynchronously, parses the requests, executes them, and writes back proper response messages.
*/
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
    while (m_socket.is_open()) {
      try {
        auto length = boost::asio::read_until(m_socket, m_buffer, '\n');
        auto buffer_begin = boost::asio::buffers_begin(m_buffer.data());
        std::string raw_query(buffer_begin, buffer_begin + static_cast<int>(length) - 1);
        m_buffer.consume(length);
        query_message query = query_message::deserialize(raw_query);
        response_message response = m_rocksdb_proxy->execute(query);
        std::string raw_response = response.serialize();
        boost::asio::write(m_socket, boost::asio::buffer(raw_response));
      } catch(const boost::wrapexcept<boost::system::system_error>& e) {
        if (e.code() == boost::asio::error::eof) {
          std::cout << "Client is finished with the queries. Closing the session..." << std::endl;
        } else {
          std::cout << "Exception thrown in handle_read: " << e.what() << std::endl;
        }
        break;
      }
    }
  }

  tcp::socket m_socket;
  std::shared_ptr<rocksdb_proxy> m_rocksdb_proxy;
  boost::asio::streambuf m_buffer;
};

/**
 * rocksdb_server is the external interface to the outside world on a tcp port.
 * 
 * It initializes the db startup, accepts the client requests asynchronously over tcp sockets, 
 *  delegates the handling of them to individual sessions.
 *  
*/
class rocksdb_server {
public:
  rocksdb_server(uint16_t port,
                 const std::string& db_path)
      : m_acceptor(io_service, tcp::endpoint(tcp::v4(), port))
      , m_socket(io_service)
      , m_rocksdb_proxy(std::make_shared<rocksdb_proxy>(db_path))
  {
    std::cout << "Starting server on port: " << port << std::endl;
    do_accept();
  }

private:
  void do_accept() {
    std::cout << "Server is waiting to accept a new request!" << std::endl;
    m_acceptor.async_accept(m_socket, [this](boost::system::error_code error_code) {
      if (!error_code) {
        auto session_ptr = std::make_shared<session>(std::move(m_socket), m_rocksdb_proxy);
        std::thread session_thread([session_ptr]() {
          session_ptr->start();
        });
        session_thread.detach();
      }
      do_accept();
    });
  }

  // tcp connection acceptor to asynchronously accept the connections and delegate the handling to sessions
  tcp::acceptor m_acceptor;
  tcp::socket m_socket;
  std::shared_ptr<rocksdb_proxy> m_rocksdb_proxy;
};

auto main(int argc, char* argv[]) -> int {
  auto args = std::span(argv, static_cast<size_t>(argc));

  try {
    assert(argc == 3 && "Usage: ./rocksdb_server <port> <db_path>");

    rocksdb_server rocksdb_server(static_cast<uint16_t>(std::stoul(args[1])), args[2]);

    // run() method is used to dequeue the async operation results and call the respective handlers.
    io_service.run();
  } catch (std::exception& e) {
    std::cerr << "Exception in Rocksdb server: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
