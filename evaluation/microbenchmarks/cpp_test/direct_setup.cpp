#include <iostream>
#include <cstdlib>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main() {
    std::cout << "Starting direct server..." << std::endl;

    // Create pipes for IPC
    int pipe_client_to_server[2];  // Client writes -> Server reads
    int pipe_server_to_client[2];  // Server writes -> Client reads

    if (pipe(pipe_client_to_server) == -1 || pipe(pipe_server_to_client) == -1) {
        std::cerr << "Failed to create pipes" << std::endl;
        return 1;
    }

    pid_t server_pid = fork();
    if (server_pid == 0) {
        // Server process
        dup2(pipe_client_to_server[0], STDIN_FILENO);  // Read from client
        dup2(pipe_server_to_client[1], STDOUT_FILENO); // Write to client

        close(pipe_client_to_server[1]);
        close(pipe_server_to_client[0]);

        execl("./direct_server", "direct_server", nullptr);
        std::cerr << "Failed to start server" << std::endl;
        return 1;
    }

    pid_t client_pid = fork();
    if (client_pid == 0) {
        // Client process
        dup2(pipe_client_to_server[1], STDOUT_FILENO); // Write to server
        dup2(pipe_server_to_client[0], STDIN_FILENO);  // Read from server

        close(pipe_client_to_server[0]);
        close(pipe_server_to_client[1]);

        execl("./direct_client", "direct_client", nullptr);
        std::cerr << "Failed to start client" << std::endl;
        return 1;
    }

    // Close unused pipe ends in the parent process
    close(pipe_client_to_server[0]);
    close(pipe_client_to_server[1]);
    close(pipe_server_to_client[0]);
    close(pipe_server_to_client[1]);

    // Wait for both child processes to finish
    waitpid(client_pid, nullptr, 0);
    waitpid(server_pid, nullptr, 0);

    std::cout << "Direct setup complete." << std::endl;
    return 0;
}

