#include <iostream>
#include <string>
#include <span>
#include <vector>
#include <memory>
#include <boost/asio.hpp>
#include <boost/asio/local/stream_protocol.hpp>
#include <thread>

#include "rocksdb_proxy.hpp"

using boost::asio::ip::tcp;
using boost::asio::local::stream_protocol;

constexpr int socket_timeout_seconds = 60; 

// io_context is the entry point to use boost's async capabilities. It is an interface to the OS I/O services.
// It manages the threads and the event loop related to connections and handler callbacks. 
// See here for more info: https://www.boost.org/doc/libs/1_65_1/doc/html/boost_asio/overview/core/basics.html
//NOLINTNEXTLINE 
boost::asio::io_context io_context;

/**
 * session class represents a connection handler for a single client.
 * 
 * Given a socket and a rocksdb_proxy, 
 *  it listens the socket asynchronously, parses the requests, executes them, and writes back proper response messages.
*/
template<typename SocketType>
class session : public std::enable_shared_from_this<session<SocketType>> {
public:
  session(SocketType socket, std::shared_ptr<rocksdb_proxy> rocksdb_proxy)
      : m_socket(std::move(socket))
      , m_rocksdb_proxy(std::move(rocksdb_proxy))
  {
  }

  void start() {
    // Set socket timeout in win and unix platforms
    // Socket timeout only applies to TCP sockets
    if constexpr (std::is_same_v<SocketType, tcp::socket>) {
      #ifdef _WIN32
      DWORD socket_timeout_ms = socket_timeout_seconds * 1000;
      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
      setsockopt(m_socket.native_handle(), SOL_SOCKET, SO_RCVTIMEO, 
                 reinterpret_cast<const char*>(&socket_timeout_ms), sizeof(socket_timeout_ms));
      #else
      // Set receive timeout of unix socket. See SO_RCVTIMEO in https://linux.die.net/man/7/socket
      struct timeval socket_timeout_val{};
      socket_timeout_val.tv_sec = socket_timeout_seconds;
      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
      setsockopt(m_socket.native_handle(), SOL_SOCKET, SO_RCVTIMEO, 
                 reinterpret_cast<const char*>(&socket_timeout_val), sizeof(socket_timeout_val));
      #endif
    }
    handle_read();
  }

private:
  void handle_read() {
    while (m_socket.is_open()) {
      try {
        // Read message length
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays)
        char length_buffer[4];  // Assuming message length is a 4-byte integer
        boost::asio::read(m_socket, boost::asio::buffer(length_buffer, 4));
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        int message_length = *reinterpret_cast<int*>(length_buffer);

        // Read actual message
        std::vector<char> message_buffer(static_cast<size_t>(message_length));
        boost::asio::read(m_socket, boost::asio::buffer(message_buffer));

        std::string raw_query(message_buffer.begin(), message_buffer.end());
        query_message query = query_message::deserialize(raw_query);
        response_message response = m_rocksdb_proxy->execute(query);
        std::string raw_response = response.serialize();

        // Send response length
        int response_size = static_cast<int>(raw_response.size());
        // Prepend response size to the response
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        raw_response.insert(0, reinterpret_cast<const char*>(&response_size), sizeof(int));

        // Send response
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

  SocketType m_socket;
  std::shared_ptr<rocksdb_proxy> m_rocksdb_proxy;
};

/**
 * rocksdb_server is the external interface to the outside world on a tcp port.
 * 
 * It initializes the db startup, accepts the client requests asynchronously over tcp sockets, 
 *  delegates the handling of them to individual sessions.
 *  
*/
template<typename AcceptorType, typename SocketType>
class rocksdb_server_base {
public:
  template<typename EndpointType>
  rocksdb_server_base(const EndpointType& endpoint, const std::string& db_path)
      : m_acceptor(io_context, endpoint)
      , m_socket(io_context)
      , m_rocksdb_proxy(std::make_shared<rocksdb_proxy>(db_path))
  {
    do_accept();
  }

private:
  void do_accept() {
    std::cout << "Server is waiting to accept a new request!" << std::endl;
    m_acceptor.async_accept(m_socket, [this](boost::system::error_code error_code) {
      if (!error_code) {
        auto session_ptr = std::make_shared<session<SocketType>>(std::move(m_socket), m_rocksdb_proxy);
        std::thread session_thread([session_ptr]() {
          session_ptr->start();
        });
        session_thread.detach();
      }
      do_accept();
    });
  }

  // tcp connection acceptor to asynchronously accept the connections and delegate the handling to sessions
  AcceptorType m_acceptor;
  SocketType m_socket;
  std::shared_ptr<rocksdb_proxy> m_rocksdb_proxy;
};

using tcp_rocksdb_server = rocksdb_server_base<tcp::acceptor, tcp::socket>;
using unix_rocksdb_server = rocksdb_server_base<stream_protocol::acceptor, stream_protocol::socket>;

auto main(int argc, char* argv[]) -> int {
  auto args = std::span(argv, static_cast<size_t>(argc));

  try {
    if (argc == 3) {
      // TCP mode: ./rocksdb_server <port> <db_path>
      std::cout << "Starting TCP server on port: " << args[1] << std::endl;
      tcp_rocksdb_server server(tcp::endpoint(tcp::v4(), static_cast<uint16_t>(std::stoul(args[1]))), args[2]);
      io_context.run();
    }
    else if (argc == 4 && std::string(args[1]) == "--unix") {
      // Unix socket mode: ./rocksdb_server --unix <socket_path> <db_path>
      std::string socket_path = args[2];
      std::cout << "Starting Unix socket server on: " << socket_path << std::endl;
      
      // Remove existing socket file if it exists
      std::remove(socket_path.c_str());
      
      unix_rocksdb_server server(stream_protocol::endpoint(socket_path), args[3]);
      io_context.run();
    }
    else {
      std::cerr << "Usage:\n";
      std::cerr << "  TCP mode: ./rocksdb_server <port> <db_path>\n";
      std::cerr << "  Unix socket mode: ./rocksdb_server --unix <socket_path> <db_path>\n";
      return 1;
    }
  } catch (std::exception& e) {
    std::cerr << "Exception in Rocksdb server: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
