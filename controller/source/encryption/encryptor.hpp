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

#define ENCRYPTION_KEY_LEN  32
#define INITIALIZATION_VECTOR_LEN 16

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

class encryptor
{
public:

  /**
   * Get singleton encryptor object.
  */
  static auto get_instance() -> encryptor* {
    static encryptor encryptor;
    return &encryptor;
  }

  /**
   * Encrypt given plain text.
   * 
   * If successful, output's first INITIALIZATION_VECTOR_LEN chars contains initialization vector (iv) in plain text.
   * The rest of the output string contains ciphered form of the input based on encryption key and randomly generated iv.
  */
  auto encrypt(const std::string& input) -> encrpyt_result
  {
    static encrpyt_result failed_encrpyt_result {{}, false};
    unsigned char initialization_vector[INITIALIZATION_VECTOR_LEN];
    RAND_bytes(initialization_vector, INITIALIZATION_VECTOR_LEN);

    EVP_CIPHER_CTX* ctx;

    /* +1 to include the null termination */
    int input_len = input.size() + 1;

    /* max ciphertext len for a n bytes of plaintext is n + AES_BLOCK_SIZE -1
     * bytes */
    unsigned char ciphertext[input_len + AES_BLOCK_SIZE];
    int temp_len;
    int ciphertext_len;
    auto input_ptr = reinterpret_cast<const unsigned char*>(input.c_str());

    /* Create and initialise the context */
    if (!(ctx = EVP_CIPHER_CTX_new())) {
      std::cout << "Could not initialize encrypt context!" << std::endl;
      return failed_encrpyt_result;
    }

    /* Initialise the encryption operation. */
    if (1
        != EVP_EncryptInit_ex(
            ctx, EVP_aes_256_gcm(), NULL, m_key, initialization_vector))
    {
      std::cout << "Could not initialize encrypt operation!" << std::endl;
      return failed_encrpyt_result;
    }

    /* Provide the plaintext to be encrypted, and obtain the encrypted output.
     */
    if (1 != EVP_EncryptUpdate(ctx, ciphertext, &temp_len, input_ptr, input_len))
    {
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
    result_string.append(reinterpret_cast<char*>(initialization_vector), INITIALIZATION_VECTOR_LEN);
    result_string.append(reinterpret_cast<char*>(ciphertext), ciphertext_len - 1);
    return encrpyt_result {result_string,true};
  }

  /**
   * Decrypt given input. 
   * 
   * First INITIALIZATION_VECTOR_LEN chars are expected to be initialization vector in plain text.
   * The rest of the characters are expected to be the cipher text
  */
  auto decrypt(const std::string& iv_and_ciphertext) -> decrpyt_result
  {
    static decrpyt_result failed_decrpyt_result {{}, false};
    unsigned char initialization_vector[INITIALIZATION_VECTOR_LEN];
    memcpy(initialization_vector, iv_and_ciphertext.c_str(), INITIALIZATION_VECTOR_LEN);
    std::string ciphertext = iv_and_ciphertext.substr(INITIALIZATION_VECTOR_LEN);

    /* +1 to include the null termination */
    int ciphertext_len = ciphertext.size() + 1;

    /* plaintext will always be equal to or lesser than the length of ciphertext
     */
    unsigned char plaintext[ciphertext_len];

    auto ciphertext_ptr = reinterpret_cast<const unsigned char*>(ciphertext.c_str());

    EVP_CIPHER_CTX* ctx;
    int temp_len;
    int plaintext_len;
    int ret;

    /* Create and initialise the context */
    if (!(ctx = EVP_CIPHER_CTX_new())) {
      std::cout << "Could not initialize decrypt context!" << std::endl;
      return failed_decrpyt_result;
    }

    /* Initialise the decryption operation. */
    if (!EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, m_key, initialization_vector))
    {
      std::cout << "Could not initialize decrypt operation!" << std::endl;
      return failed_decrpyt_result;
    }

    /* Provide the ciphertext to be decrypted, and obtain the plaintext. */
    if (!EVP_DecryptUpdate(
            ctx, plaintext, &temp_len, ciphertext_ptr, ciphertext_len))
    {
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

    /* Return successful decrypt result. Provide (plaintext_len - 1) as length
     * to exclude null termination */
    return decrpyt_result {
        std::string(reinterpret_cast<char*>(plaintext), plaintext_len - 1),
        true};
  }

  auto init_encryption_key(const std::optional<std::string>& encryption_key = std::nullopt) -> void {
    if (encryption_key.has_value()) {
      if (encryption_key.value().size() != ENCRYPTION_KEY_LEN) {
        std::cout << "Failed to set encryption key. Expected length is " << ENCRYPTION_KEY_LEN 
                  << ", given length is " <<  encryption_key.value().size()
                  << ". Falling back to the default key." << std::endl;
        return;
      }
      memcpy(m_key, encryption_key.value().c_str(), ENCRYPTION_KEY_LEN);
    }
  }

  private:
    encryptor() = default;

    unsigned char m_key[ENCRYPTION_KEY_LEN] = "0123456789012345678901234567890";
};



} // namespace controller