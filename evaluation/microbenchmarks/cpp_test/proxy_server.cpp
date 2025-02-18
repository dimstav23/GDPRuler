#include <iostream>
#include <unistd.h>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstring>

#define PROXY_PORT 8081  // Proxy server port

int main() {
    // Create socket for proxy server
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        std::cerr << "Failed to create socket." << std::endl;
        return 1;
    }

    sockaddr_in server_addr {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PROXY_PORT);

    // Bind socket
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Bind failed." << std::endl;
        return 1;
    }

    // Listen for incoming connections
    if (listen(server_socket, 3) < 0) {
        std::cerr << "Listen failed." << std::endl;
        return 1;
    }

    std::cout << "Proxy server listening on port " << PROXY_PORT << "..." << std::endl;

    int client_socket = accept(server_socket, nullptr, nullptr);
    if (client_socket < 0) {
        std::cerr << "Accept failed." << std::endl;
        return 1;
    }

    char buffer[1024];
    std::string response = "true";
    size_t response_length = response.length();

    while (true) {
        ssize_t bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
        if (bytes_received <= 0) {
            break;  // client closed connection or error occurred
        }

        // Always return "true" as the response
        send(client_socket, response.data(), response_length, 0);
    }

    close(client_socket);
    close(server_socket);

    return 0;
}
