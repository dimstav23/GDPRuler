#include "Crypto.hpp"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <stdexcept>
#include <cstring>
#include <iostream>

Crypto::Crypto()
{
    // Initialize OpenSSL
    OpenSSL_add_all_algorithms();
}

Crypto::~Crypto()
{
    // Clean up OpenSSL
    // EVP_cleanup(); // deprecated
}

EVP_CIPHER_CTX* Crypto::getEncryptContext()
{
    static thread_local EVP_CIPHER_CTX* encrypt_ctx = nullptr;

    if (!encrypt_ctx) {
        encrypt_ctx = EVP_CIPHER_CTX_new();
        if (!encrypt_ctx) {
            throw std::runtime_error("Failed to create thread-local encryption context");
        }
        
        // Pre-initialize with cipher
        if (EVP_EncryptInit_ex(encrypt_ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
            EVP_CIPHER_CTX_free(encrypt_ctx);
            encrypt_ctx = nullptr;
            throw std::runtime_error("Failed to pre-initialize encryption context");
        }
    }
    return encrypt_ctx;
}

EVP_CIPHER_CTX* Crypto::getDecryptContext()
{
    static thread_local EVP_CIPHER_CTX* decrypt_ctx = nullptr;
    
    if (!decrypt_ctx) {
        decrypt_ctx = EVP_CIPHER_CTX_new();
        if (!decrypt_ctx) {
            throw std::runtime_error("Failed to create thread-local decryption context");
        }
        
        // Pre-initialize with cipher
        if (EVP_DecryptInit_ex(decrypt_ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
            EVP_CIPHER_CTX_free(decrypt_ctx);
            decrypt_ctx = nullptr;
            throw std::runtime_error("Failed to pre-initialize decryption context");
        }
    }
    return decrypt_ctx;
}

// Encrypt data using AES-256-GCM with provided IV
std::vector<uint8_t> Crypto::encrypt(std::vector<uint8_t> &&plaintext,
                                     const std::vector<uint8_t> &key,
                                     const std::vector<uint8_t> &iv)
{
    // Add size debugging at the start
    // std::cout << "Crypto::encrypt - Input size: " << plaintext.size() << " bytes" << std::endl;
    
    if (plaintext.empty()) {
        // std::cout << "Crypto::encrypt - Empty input, returning empty" << std::endl;
        return {};
    }
    
    if (key.size() != KEY_SIZE)
        throw std::runtime_error("Invalid key size");
    if (iv.size() != GCM_IV_SIZE)
        throw std::runtime_error("Invalid IV size");

    // Get the initialized encryption context with the cipher
    EVP_CIPHER_CTX* m_encryptCtx = getEncryptContext();
    // EVP_CIPHER_CTX_reset(m_encryptCtx);

    // Initialize encryption operation
    if (EVP_EncryptInit_ex(m_encryptCtx, nullptr, nullptr, key.data(), iv.data()) != 1)
    {
        throw std::runtime_error("Failed to initialize encryption");
    }

    // Calculate the exact output size: size_field + ciphertext + tag
    // For GCM mode, ciphertext size equals plaintext size (no padding)
    const size_t sizeFieldSize = sizeof(uint32_t);
    const size_t ciphertextSize = plaintext.size();
    const size_t totalSize = sizeFieldSize + ciphertextSize + GCM_TAG_SIZE;

    // std::cout << "Crypto::encrypt - Expected output size: " << totalSize 
    //           << " bytes (sizeField=" << sizeFieldSize 
    //           << " + ciphertext=" << ciphertextSize 
    //           << " + tag=" << GCM_TAG_SIZE << ")" << std::endl;

    // Pre-allocate result buffer with exact final size
    std::vector<uint8_t> result(totalSize);

    // Reserve space for data size field
    uint32_t dataSize = ciphertextSize;
    std::memcpy(result.data(), &dataSize, sizeFieldSize);

    // Perform encryption directly into the result buffer (after the size field)
    int encryptedLen = 0;
    if (EVP_EncryptUpdate(m_encryptCtx, result.data() + sizeFieldSize, &encryptedLen,
                          plaintext.data(), plaintext.size()) != 1)
    {
        throw std::runtime_error("Failed during encryption update");
    }

    // std::cout << "Crypto::encrypt - After EncryptUpdate: " << encryptedLen << " bytes" << std::endl;

    // Finalize encryption (writing to the buffer right after the existing encrypted data)
    int finalLen = 0;
    if (EVP_EncryptFinal_ex(m_encryptCtx, result.data() + sizeFieldSize + encryptedLen, &finalLen) != 1)
    {
        throw std::runtime_error("Failed to finalize encryption");
    }

    // std::cout << "Crypto::encrypt - After EncryptFinal: " << finalLen << " additional bytes" << std::endl;

    // Sanity check: for GCM, encryptedLen + finalLen should equal plaintext.size()
    if (encryptedLen + finalLen != static_cast<int>(plaintext.size()))
    {
        // std::cout << "Crypto::encrypt - ERROR: Size mismatch! Expected " << plaintext.size()
        //           << ", got " << (encryptedLen + finalLen) << std::endl;
        throw std::runtime_error("Unexpected encryption output size");
    }

    // Get the authentication tag and write it directly to the result buffer
    if (EVP_CIPHER_CTX_ctrl(m_encryptCtx, EVP_CTRL_GCM_GET_TAG, GCM_TAG_SIZE,
                            result.data() + sizeFieldSize + ciphertextSize) != 1)
    {
        throw std::runtime_error("Failed to get authentication tag");
    }

    // std::cout << "Crypto::encrypt - Final output size: " << result.size() << " bytes" << std::endl;
    return result;
}

// Decrypt data using AES-256-GCM with provided IV
std::vector<uint8_t> Crypto::decrypt(const std::vector<uint8_t> &encryptedData,
                                     const std::vector<uint8_t> &key,
                                     const std::vector<uint8_t> &iv)
{
    // std::cout << "Crypto::decrypt - Input size: " << encryptedData.size() << " bytes" << std::endl;
    try
    {
        if (encryptedData.empty())
        {
            // std::cout << "Crypto::decrypt - Empty input, returning empty" << std::endl;
            return std::vector<uint8_t>();
        }

        // Validate key size
        if (key.size() != KEY_SIZE)
        {
            throw std::runtime_error("Invalid key size. Expected 32 bytes for AES-256");
        }

        // Validate IV size
        if (iv.size() != GCM_IV_SIZE)
        {
            throw std::runtime_error("Invalid IV size. Expected 12 bytes for GCM");
        }

        // Ensure we have at least enough data for the data size field
        if (encryptedData.size() < sizeof(uint32_t))
        {
            throw std::runtime_error("Encrypted data too small - missing data size");
        }

        // Extract the encrypted data size
        uint32_t dataSize;
        std::memcpy(&dataSize, encryptedData.data(), sizeof(dataSize));
        size_t position = sizeof(dataSize);

        // std::cout << "Crypto::decrypt - Extracted data size: " << dataSize << " bytes" << std::endl;
        // std::cout << "Crypto::decrypt - Available for ciphertext+tag: " << (encryptedData.size() - position) << " bytes" << std::endl;

        // Validate data size
        if (position + dataSize > encryptedData.size())
        {
            // std::cout << "Crypto::decrypt - ERROR: Need " << (position + dataSize) 
            //           << " bytes, but only have " << encryptedData.size() << std::endl;
            throw std::runtime_error("Encrypted data too small - missing complete data");
        }

        // Extract the encrypted data
        std::vector<uint8_t> ciphertext(dataSize);
        std::memcpy(ciphertext.data(), encryptedData.data() + position, dataSize);
        position += dataSize;

        // std::cout << "Crypto::decrypt - Extracted ciphertext: " << ciphertext.size() << " bytes" << std::endl;

        // Extract the authentication tag
        if (position + GCM_TAG_SIZE > encryptedData.size())
        {
            // std::cout << "Crypto::decrypt - ERROR: Need " << (position + GCM_TAG_SIZE) 
            //           << " bytes for tag, but only have " << encryptedData.size() << std::endl;
            throw std::runtime_error("Encrypted data too small - missing authentication tag");
        }

        std::vector<uint8_t> tag(GCM_TAG_SIZE);
        std::memcpy(tag.data(), encryptedData.data() + position, GCM_TAG_SIZE);

        // std::cout << "Crypto::decrypt - Extracted tag: " << tag.size() << " bytes" << std::endl;
        
        // Get the initialized encryption context with the cipher
        EVP_CIPHER_CTX* m_decryptCtx = getDecryptContext();

        // Initialize decryption operation
        if (EVP_DecryptInit_ex(m_decryptCtx, nullptr, nullptr, key.data(), iv.data()) != 1)
        {
            throw std::runtime_error("Failed to initialize decryption");
        }

        // Set expected tag value
        if (EVP_CIPHER_CTX_ctrl(m_decryptCtx, EVP_CTRL_GCM_SET_TAG, GCM_TAG_SIZE, tag.data()) != 1)
        {
            throw std::runtime_error("Failed to set authentication tag");
        }

        // Prepare output buffer for plaintext
        std::vector<uint8_t> decryptedData(ciphertext.size());
        int decryptedLen = 0;

        // Perform decryption
        if (EVP_DecryptUpdate(m_decryptCtx, decryptedData.data(), &decryptedLen,
                              ciphertext.data(), ciphertext.size()) != 1)
        {
            throw std::runtime_error("Failed during decryption update");
        }

        // std::cout << "Crypto::decrypt - After DecryptUpdate: " << decryptedLen << " bytes" << std::endl;

        // Finalize decryption and verify tag
        int finalLen = 0;
        int ret = EVP_DecryptFinal_ex(m_decryptCtx, decryptedData.data() + decryptedLen, &finalLen);

        // std::cout << "Crypto::decrypt - After DecryptFinal: " << finalLen << " additional bytes (ret=" << ret << ")" << std::endl;

        if (ret != 1)
        {
            throw std::runtime_error("Authentication failed: data may have been tampered with");
        }

        // Resize the decrypted data to the actual length
        decryptedData.resize(decryptedLen + finalLen);

        // std::cout << "Crypto::decrypt - Final output size: " << decryptedData.size() << " bytes" << std::endl;
        return decryptedData;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error decrypting data: " << e.what() << std::endl;
        // Print OpenSSL error queue
        ERR_print_errors_fp(stderr);
        return std::vector<uint8_t>();
    }
}
