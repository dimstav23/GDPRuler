#pragma once

#include <cstring>
#include <optional>
#include <string>
#include <array>
#include <vector>

#ifndef NDEBUG
#include "cipher_print_utils.hpp"
#endif

#include <openssl/aes.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#ifndef ENCRYPTION_ENABLED
  #define ENCRYPTION_ENABLED
#endif

// Encryption key types
enum class cipher_key_type {
  db_key,   // Key type for the database
  log_key,  // Key type for the log
  max_key   // Maximum key value for range checking
};

namespace controller
{

constexpr int encryption_key_len = 16; // 128 bits for AES-GCM-128
constexpr int initialization_vector_len = 12; // 96 bits for AES-GCM
constexpr int tag_len = 16; // 128 bits for AES-GCM-128

class encrypt_result
{
public:
  encrypt_result(std::string ciphertext, bool success)
      : m_ciphertext {std::move(ciphertext)}
      , m_success {success}
  {
  }

  std::string m_ciphertext;
  bool m_success;
};

class decrypt_result
{
public:
  decrypt_result(std::string plaintext, bool success)
      : m_plaintext {std::move(plaintext)}
      , m_success {success}
  {
  }

  std::string m_plaintext;
  bool m_success;
};

/*
 * cipher engine class: Implements the encrypt/decrypt functions
 * Note: Suppress linting from this class due to the c-style argument passing 
 * of OpenSSL functions
 */
class cipher_engine
{
// NOLINTBEGIN
public:

  /**
   * Get singleton cipher_engine object.
  */
  static auto get_instance() -> cipher_engine* {
    static cipher_engine cipher_inst;
    return &cipher_inst;
  }

  /**
   * Encrypts the input using the specified encryption key type.
   * 
   * If successful, output's first initialization_vector_len chars contain 
   * the initialization vector (iv) in plain text.
   * Following tag_len chars contain calculated MAC. 
   * Next 4 bytes contain the size of the encrypted value.
   * The rest of the output string contains ciphered form of the input based on encryption key.
   *
   * @param input The plaintext to encrypt.
   * @param keyType The encryption key type to use.
   * @return An encrypt_result object containing the ciphertext and success status.
   */
  auto encrypt(const std::string& input, cipher_key_type key_type) -> encrypt_result {
    static encrypt_result failed_encrypt_result {{}, /*success*/ false};
    
    const unsigned char* key = get_encryption_key(key_type);
    if (key == nullptr) {
      std::cerr << "Invalid encryption key type!" << std::endl;
      return failed_encrypt_result;
    }

    /* Generate a random IV */
    std::array<unsigned char, initialization_vector_len> initialization_vector{};
    if (RAND_bytes(initialization_vector.data(), sizeof(initialization_vector)) != 1) {
      std::cerr << "Failed to generate random initialization vector!" << std::endl;
      return failed_encrypt_result;
    }

    /* Create and initialise the context */
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (ctx == nullptr) {
      std::cerr << "Failed to create encryption context!" << std::endl;
      return failed_encrypt_result;
    }

    /* Initialise the encryption operation. */
    if (EVP_EncryptInit_ex(ctx, EVP_aes_128_gcm(), nullptr, key, initialization_vector.data()) != 1) {
      std::cerr << "Failed to initialize encryption!" << std::endl;
      EVP_CIPHER_CTX_free(ctx);
      return failed_encrypt_result;
    }

    int ciphertext_len = 0;
    /* max ciphertext len for a n bytes of plaintext is n + AES_BLOCK_SIZE bytes */
    std::vector<unsigned char> ciphertext(input.size() + static_cast<size_t>(EVP_CIPHER_CTX_block_size(ctx)));
    /* Provide the plaintext to be encrypted, and obtain the encrypted output. */
    if (EVP_EncryptUpdate(ctx, ciphertext.data(), &ciphertext_len, reinterpret_cast<const unsigned char*>(input.data()), static_cast<int>(input.size())) != 1) {
      std::cerr << "Failed to perform encryption!" << std::endl;
      EVP_CIPHER_CTX_free(ctx);
      return failed_encrypt_result;
    }

    /* Finalise the encryption. */
    int final_len = 0;
    if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + ciphertext_len, &final_len) != 1) {
      std::cerr << "Failed to finalize encryption!" << std::endl;
      EVP_CIPHER_CTX_free(ctx);
      return failed_encrypt_result;
    }

    ciphertext_len += final_len;

    /* Retrieve the MAC (Message Authentication Code) of the ciphertext */
    std::vector<unsigned char> mac(tag_len);
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, tag_len, mac.data()) != 1) {
      std::cerr << "Failed to extract MAC!" << std::endl;
      EVP_CIPHER_CTX_free(ctx);
      return failed_encrypt_result;
    }

    /* Clean up */
    EVP_CIPHER_CTX_free(ctx);

    std::string result_string;
    result_string.append(reinterpret_cast<const char*>(initialization_vector.data()), initialization_vector_len);
    result_string.append(reinterpret_cast<const char*>(mac.data()), tag_len);

    // Encode the ciphertext length as a 4-byte integer
    result_string.append(reinterpret_cast<const char*>(&ciphertext_len), sizeof(int));

    result_string.append(reinterpret_cast<const char*>(ciphertext.data()), 
                         static_cast<std::string::size_type>(ciphertext_len));

    // std::cout << "iv : ";
    // print_bytes(initialization_vector);
    // std::cout << "mac : ";
    // print_bytes(mac);
    // std::cout << "len: " << ciphertext_len << "  ";
    // print_int_bytes(ciphertext_len);
    // std::cout << "ciphertext : ";
    // print_bytes(ciphertext);

    return encrypt_result {result_string, /*success*/ true};
  }


  /**
   * Decrypts the ciphertext using the specified encryption key type.
   * 
   * First initialization_vector_len chars are expected to be the initialization vector,
   * the calculated MAC, the size of the encrypted value, and the actual encrypted value.
   * 
   * @param ciphertext The ciphertext to decrypt.
   * @param keyType The encryption key type to use.
   * @return A decrypt_result object containing the plaintext and success status.
   */
  auto decrypt(const std::string& ciphertext, cipher_key_type key_type) -> decrypt_result {
    static decrypt_result failed_decrypt_result {{}, /*success*/ false};

    const unsigned char* key = get_encryption_key(key_type);
    if (key == nullptr) {
      std::cerr << "Invalid encryption key type!" << std::endl;
      return failed_decrypt_result;
    }

    // Extract the components from the result string
    const auto* iv = reinterpret_cast<const unsigned char*>(ciphertext.data());
    const auto* mac = iv + initialization_vector_len;
    const auto* len_bytes = mac + tag_len;
    const auto* encrypted_value = len_bytes + sizeof(int);

    // Retrieve the ciphertext length from the 4-byte integer
    int ciphertext_len = 0;
    std::copy_n(len_bytes, sizeof(int), reinterpret_cast<unsigned char*>(&ciphertext_len));

    // std::cout << "to_decrypt: ";
    // print_bytes(ciphertext);
    // std::cout << "iv: ";
    // print_buffer_bytes(iv, initialization_vector_len);
    // std::cout << "mac: ";
    // print_buffer_bytes(mac, tag_len);
    // std::cout << "len: " << ciphertext_len << "  ";
    // print_int_bytes(ciphertext_len);
    // std::cout << "encrypted_value: ";
    // print_buffer_bytes(encrypted_value, static_cast<unsigned int>(ciphertext_len));

    // Create and initialize the context
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (ctx == nullptr) {
        std::cerr << "Failed to create decryption context!" << std::endl;
        return failed_decrypt_result;
    }

    // Initialize the decryption operation
    if (EVP_DecryptInit_ex(ctx, EVP_aes_128_gcm(), nullptr, key, iv) != 1) {
        std::cerr << "Failed to initialize decryption!" << std::endl;
        EVP_CIPHER_CTX_free(ctx);
        return failed_decrypt_result;
    }

    // Provide the ciphertext to be decrypted, and obtain the decrypted output
    int plaintext_len = 0;
    std::vector<unsigned char> plaintext(static_cast<unsigned int>(ciphertext_len));
    if (EVP_DecryptUpdate(ctx, plaintext.data(), &plaintext_len, encrypted_value, ciphertext_len) != 1) {
        std::cerr << "Failed to perform decryption!" << std::endl;
        EVP_CIPHER_CTX_free(ctx);
        return failed_decrypt_result;
    }

    // Set the expected MAC value
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, tag_len, const_cast<unsigned char*>(mac)) != 1) {
        std::cerr << "Failed to set MAC value!" << std::endl;
        EVP_CIPHER_CTX_free(ctx);
        return failed_decrypt_result;
    }

    // Finalize the decryption
    int final_len = 0;
    if (EVP_DecryptFinal_ex(ctx, plaintext.data() + plaintext_len, &final_len) != 1) {
        std::cerr << "Failed to finalize decryption!" << std::endl;
        EVP_CIPHER_CTX_free(ctx);
        return failed_decrypt_result;
    }
    plaintext_len += final_len;

    // Clean up
    EVP_CIPHER_CTX_free(ctx);

    auto result_string = std::string(reinterpret_cast<const char*>(plaintext.data()), 
                                     static_cast<std::string::size_type>(plaintext_len));
    
    return decrypt_result { result_string, /*success*/ true};
  }

  /**
   * Initializes the encryption key for the specified key type.
   * If no encryption key is provided, it falls back to the default test key.
   *
   * @param encryption_key The encryption key to set.
   * @param keyType The encryption key type to initialize.
   */
  auto init_encryption_key(const std::optional<std::string>& encryption_key = std::nullopt,
                          cipher_key_type key_type = cipher_key_type::max_key) -> bool 
  {
    if (encryption_key.has_value()) {
      const unsigned char* key = get_encryption_key(key_type);
      if (key == nullptr) {
        std::cerr << "Invalid encryption key type!" << std::endl;
        return false;
      }

      if (encryption_key.value().size() != encryption_key_len) {
        std::cerr << "Failed to set encryption key. Expected length is " << encryption_key_len 
                  << ", given length is " <<  encryption_key.value().size()
                  << ". Falling back to the default key." << std::endl;
        return false;
      }
      memcpy(const_cast<unsigned char*>(key), encryption_key.value().c_str(), encryption_key_len);
    }
    return true;
  }

  private:
    cipher_engine() = default;
    // Default database encryption key
    unsigned char m_db_key[encryption_key_len] = "012345678901234";
    // Default gdpr log encryption key
    unsigned char m_log_key[encryption_key_len] = "123401234567890";
    // Array of encryption key
    const unsigned char* m_encryption_keys[2] = { m_db_key, m_log_key }; 

    /**
     * Retrieves the encryption key based on the specified key type.
     *
     * @param keyType The encryption key type.
     * @return The encryption key for the specified key type.
     */
    const unsigned char* get_encryption_key(cipher_key_type key_type) const {
      if (key_type >= cipher_key_type::max_key) {
        std::cerr << "Invalid encryption key type!" << std::endl;
        return nullptr;
      }
      return m_encryption_keys[static_cast<int>(key_type)];
    }

// NOLINTEND
};


} // namespace controller