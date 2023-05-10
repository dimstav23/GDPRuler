#include <iostream>
#include "encryption/cipher_engine.hpp"

using controller::cipher_engine;

auto main() -> int
{
    // Set the encryption key
    cipher_engine::get_instance()->init_encryption_key("0123456789012345");

    // Get the instance of the cipher_engine class
    cipher_engine* encryption = cipher_engine::get_instance();

    // Input plaintext
    std::string plaintext = "Hello, world!";
    std::cout << "Plain text: " << plaintext << std::endl;

    // Encrypt the plaintext
    auto encrypt_result = encryption->encrypt(plaintext);

    // Check if encryption was successful
    if (encrypt_result.m_success) {
        std::cout << "Encryption successful!" << std::endl;
        std::cout << "Result string: " << std::flush;
        std::cout << encrypt_result.m_ciphertext << std::endl;

        // Decrypt the ciphertext
        auto decryption_res = encryption->decrypt(encrypt_result.m_ciphertext);

        // Check if decryption was successful
        if (decryption_res.m_success) {
            std::cout << "Decryption successful!" << std::endl;
            std::cout << "Decrypted text: " << decryption_res.m_plaintext << std::endl;
        } else {
            std::cout << "Decryption failed!" << std::endl;
        }
    } else {
        std::cout << "Encryption failed!" << std::endl;
    }

    return 0;
}