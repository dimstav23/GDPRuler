#include <iostream>
#include <cstdlib>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main() {
    std::cout << "Starting proxy setup..." << std::endl;

    pid_t proxy_server_pid = fork();
    if (proxy_server_pid == 0) {
        // Proxy server process
        execl("./proxy_server", "proxy_server", nullptr);
        std::cerr << "Failed to start proxy server" << std::endl;
        return 1;
    }

    // Allow the server to start listening before the client tries to connect
    sleep(1);  // Wait for the server to start

    pid_t proxy_client_pid = fork();
    if (proxy_client_pid == 0) {
        // Proxy client process
        execl("./proxy_client", "proxy_client", nullptr);
        std::cerr << "Failed to start proxy client" << std::endl;
        return 1;
    }

    // Wait for both child processes to finish
    waitpid(proxy_client_pid, nullptr, 0);
    waitpid(proxy_server_pid, nullptr, 0);

    std::cout << "Proxy setup complete." << std::endl;
    return 0;
}

