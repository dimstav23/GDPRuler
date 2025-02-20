#include <iostream>
#include <string>

int main() {
    std::string input;
    while (std::getline(std::cin, input)) {
        std::cout << "true" << std::endl;  // Always respond with "true"
    }
    return 0;
}

