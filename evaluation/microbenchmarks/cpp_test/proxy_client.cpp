#include <iostream>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <cstring>
#include <unistd.h>
#include <chrono>
#include <random>

#define PORT 8081  // Proxy server port

#define BUFFER_SIZE 64000

constexpr int N = 1000000;
constexpr int KEY_SIZE = 16;
constexpr int VALUE_SIZE = 64;

// Function to generate a random alphanumeric string of given size
std::string random_string(size_t length) {
    static const char characters[] =
        "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    static std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<> dist(0, sizeof(characters) - 2);

    std::string result;
    result.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        result += characters[dist(rng)];
    }
    return result;
}

int main() {
    // Create socket
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        std::cerr << "Failed to create socket." << std::endl;
        return 1;
    }

    sockaddr_in server_addr {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    int buffer_size = BUFFER_SIZE;
    int return_code = setsockopt(
		client_socket,
		SOL_SOCKET,
		SO_RCVBUF,
		&buffer_size,
		sizeof buffer_size
	  );
    return_code = setsockopt(
		client_socket,
		SOL_SOCKET,
		SO_SNDBUF,
		&buffer_size,
		sizeof buffer_size
	  );

    int tcpnodelay = 1;
    if (setsockopt(client_socket, IPPROTO_TCP, TCP_NODELAY, &tcpnodelay, sizeof(tcpnodelay))) {
      std::cerr << "Failed to set TCP_NODELAY option" << std::endl;
      close(client_socket);
      return 1;
    }

    // Connect to the proxy server
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Connection to proxy server failed." << std::endl;
        return 1;
    }

    std::string key = random_string(KEY_SIZE);
    std::string value = random_string(VALUE_SIZE);
    std::string request = key + " " + value + "\n";
    size_t request_length = request.length();

    char buffer[1024] = {0};
    
    auto start_time = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < N; ++i) {
        send(client_socket, request.data(), request_length, 0);
        
        ssize_t bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
        if (bytes_received <= 0) {
            std::cerr << "Failed to receive response." << std::endl;
            break;
        }
        
        if (std::strncmp(buffer, "true", 4) != 0) {
            std::cerr << "Unexpected response: " << buffer << std::endl;
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    double elapsed_time = std::chrono::duration<double>(end_time - start_time).count();
    std::cout << "Proxy client time: " << elapsed_time << " seconds" << std::endl;

    close(client_socket);
    return 0;
}

