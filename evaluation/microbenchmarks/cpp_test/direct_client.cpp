#include <iostream>
#include <string>
#include <random>
#include <chrono>

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
    std::string key = random_string(KEY_SIZE);
    std::string value = random_string(VALUE_SIZE);
    auto start_time = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < N; ++i) {
        std::cout << key << " " << value << std::endl;
        std::cout.flush();

        std::string response;
        std::getline(std::cin, response);

        if (response != "true") {
            std::cerr << "Unexpected response: " << response << std::endl;
            break;
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    double elapsed_time = std::chrono::duration<double>(end_time - start_time).count();
    // print to stderr as the stdout is redirected to the server
    std::cerr << "Direct client time: " << elapsed_time << " seconds" << std::endl;

    return 0;
}

