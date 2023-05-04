#pragma once

#include <cstring>
#include <iostream>
#include <optional>
#include <string>

#include <openssl/aes.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#ifndef ENCRYPTION_ENABLED
  #define ENCRYPTION_ENABLED
#endif

namespace controller
{

constexpr int cryption_key_len = 32;
constexpr int initialization_vector_len = 16;

class encrpyt_result
{
public:
  encrpyt_result(std::string ciphertext, bool success)
      : m_ciphertext {std::move(ciphertext)}
      , m_success {success}
  {
  }

  std::string m_ciphertext;
  bool m_success;
};

class decrpyt_result
{
public:
  decrpyt_result(std::string plaintext, bool success)
      : m_plaintext {std::move(plaintext)}
      , m_success {success}
  {
  }

  std::string m_plaintext;
  bool m_success;
};

class cryptor
{
// NOLINTBEGIN
public:

  /**
   * Get singleton cryptor object.
  */
  static auto get_instance() -> cryptor* {
    static cryptor encryptor;
    return &encryptor;
  }

  /**
   * Encrypt given plain text.
   * 
   * If successful, output's first initialization_vector_len chars contains initialization vector (iv) in plain text.
   * The rest of the output string contains ciphered form of the input based on encryption key and randomly generated iv.
  */
  auto encrypt(const std::string& input) -> encrpyt_result {
    static encrpyt_result failed_encrpyt_result {{}, /*success*/ false};
    unsigned char initialization_vector[initialization_vector_len];
    RAND_bytes(initialization_vector, initialization_vector_len);

    EVP_CIPHER_CTX* ctx = nullptr;

    /* +1 to include the null termination */
    int input_len = static_cast<int>(input.size()) + 1;

    /* max ciphertext len for a n bytes of plaintext is n + AES_BLOCK_SIZE -1 bytes */
    auto ciphertext = new unsigned char[static_cast<size_t>(input_len + AES_BLOCK_SIZE)];
    int temp_len, ciphertext_len;
    auto input_ptr = reinterpret_cast<const unsigned char*>(input.c_str());

    /* Create and initialise the context */
    if (!(ctx = EVP_CIPHER_CTX_new())) {
      std::cout << "Could not initialize encrypt context!" << std::endl;
      return failed_encrpyt_result;
    }

    /* Initialise the encryption operation. */
    if (1 != EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, m_key, initialization_vector)) {
      std::cout << "Could not initialize encrypt operation!" << std::endl;
      return failed_encrpyt_result;
    }

    /* Provide the plaintext to be encrypted, and obtain the encrypted output.
     */
    if (1 != EVP_EncryptUpdate(ctx, ciphertext, &temp_len, input_ptr, input_len)) {
      std::cout << "Could not update encrypt!" << std::endl;
      return failed_encrpyt_result;
    }
    ciphertext_len = temp_len;

    /* Finalise the encryption. */
    if (1 != EVP_EncryptFinal_ex(ctx, ciphertext + temp_len, &temp_len)) {
      std::cout << "Could not finalize encrypt!" << std::endl;
      return failed_encrpyt_result;
    }
    ciphertext_len += temp_len;

    /* Clean up */
    EVP_CIPHER_CTX_free(ctx);

    /* Return successful encrypt result. Exclude null termination */
    std::string result_string{};
    result_string.append(reinterpret_cast<char*>(initialization_vector), initialization_vector_len);
    result_string.append(reinterpret_cast<char*>(ciphertext), static_cast<size_t>(ciphertext_len - 1));
    delete ciphertext;
    return encrpyt_result {result_string,true};
  }

  /**
   * Decrypt given input. 
   * 
   * First initialization_vector_len chars are expected to be initialization vector in plain text.
   * The rest of the characters are expected to be the cipher text
  */
  auto decrypt(const std::string& iv_and_ciphertext) -> decrpyt_result {
    static decrpyt_result failed_decrpyt_result {{}, false};
    unsigned char initialization_vector[initialization_vector_len];
    memcpy(initialization_vector, iv_and_ciphertext.c_str(), initialization_vector_len);
    std::string ciphertext = iv_and_ciphertext.substr(initialization_vector_len);

    /* +1 to include the null termination */
    int ciphertext_len = static_cast<int>(ciphertext.size()) + 1;

    /* plaintext will always be equal to or lesser than the length of ciphertext
     */
    auto plaintext = new unsigned char[static_cast<size_t>(ciphertext_len)];

    auto ciphertext_ptr = reinterpret_cast<const unsigned char*>(ciphertext.c_str());

    EVP_CIPHER_CTX* ctx;
    int temp_len, plaintext_len;

    /* Create and initialise the context */
    if (!(ctx = EVP_CIPHER_CTX_new())) {
      std::cout << "Could not initialize decrypt context!" << std::endl;
      return failed_decrpyt_result;
    }

    /* Initialise the decryption operation. */
    if (!EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, m_key, initialization_vector)) {
      std::cout << "Could not initialize decrypt operation!" << std::endl;
      return failed_decrpyt_result;
    }

    /* Provide the ciphertext to be decrypted, and obtain the plaintext. */
    if (!EVP_DecryptUpdate(ctx, plaintext, &temp_len, ciphertext_ptr, ciphertext_len)) {
      std::cout << "Could not update decrypt!" << std::endl;
      return failed_decrpyt_result;
    }
    plaintext_len = temp_len;

    /* Finalise the decryption. The plaintext is decrypted and the length of the
     * plaintext is returned. */
    EVP_DecryptFinal_ex(ctx, plaintext + temp_len, &temp_len);

    /* Clean up */
    EVP_CIPHER_CTX_free(ctx);

    plaintext_len += temp_len;

    /* Return successful decrypt result. Provide (plaintext_len - 1) as length to exclude null termination */
    auto plaintext_str = std::string(reinterpret_cast<char*>(plaintext), static_cast<size_t>(plaintext_len - 1));
    delete plaintext;
    return decrpyt_result { plaintext_str, true };
  }

  auto init_cryption_key(const std::optional<std::string>& encryption_key = std::nullopt) -> void {
    if (encryption_key.has_value()) {
      if (encryption_key.value().size() != cryption_key_len) {
        std::cout << "Failed to set cryption key. Expected length is " << cryption_key_len 
                  << ", given length is " <<  encryption_key.value().size()
                  << ". Falling back to the default key." << std::endl;
        return;
      }
      memcpy(m_key, encryption_key.value().c_str(), cryption_key_len);
    }
  }

  private:
    cryptor() = default;

    unsigned char m_key[cryption_key_len] = "0123456789012345678901234567890";
  // NOLINTEND
};

} // namespace controller