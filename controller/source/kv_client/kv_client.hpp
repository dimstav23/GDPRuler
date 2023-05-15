#pragma once

#include <iostream>
#include <string>
#include <optional>

#include "../encryption/cipher_engine.hpp"

class kv_client
{
public:
  /* kv_client interface signatures */
  auto gdpr_get(const std::string& key) -> std::optional<std::string> {
    #ifndef ENCRYPTION_ENABLED
      // get the value directly w/o decryption
      return get(key);
    #else
      // get the value after decryption
      auto encrypted_value = get(key);
      if (!encrypted_value.has_value()) {
        return std::nullopt;
      }

      auto decrypt_result = m_cipher->decrypt(encrypted_value.value(), cipher_key_type::db_key);
      if (decrypt_result.m_success) {
        return decrypt_result.m_plaintext;
      }
      std::cerr << "Error in get: Decryption failed for value: " << encrypted_value.value() << std::endl;
      return std::nullopt;
    #endif
  }

  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
  auto gdpr_put(const std::string& key, const std::string& value) -> bool {
    #ifndef ENCRYPTION_ENABLED
      // put the pair directly w/o encryption
      return put(key, value);
    #else
      // put the pair after encryption
      auto encrypt_result = m_cipher->encrypt(value, cipher_key_type::db_key);
      if (encrypt_result.m_success) {
        return put(key, encrypt_result.m_ciphertext);
      }
      std::cerr << "Error in put: Encryption failed for value: " << value << std::endl;
      return false;
    #endif
  }

  auto gdpr_del(const std::string& key) -> bool {
    #ifndef ENCRYPTION_ENABLED
      // delete the pair directly w/o decryption
      return del(key);
    #else
      // delete the pair directly w/o decryption
      return del(key);
    #endif
  }

  /* Constructors, destructors, etc */
  virtual ~kv_client() = default;
  kv_client() = default;
  kv_client(const kv_client&) = default;
  auto operator=(kv_client const&) -> kv_client& = default;
  kv_client(kv_client&&) = default;
  auto operator=(kv_client&&) -> kv_client& = default;

protected:
  /* kv_client interface signatures */
  virtual auto get(const std::string& key) -> std::optional<std::string> = 0;
  virtual auto put(const std::string& key, const std::string& value) -> bool = 0;
  virtual auto del(const std::string& key) -> bool = 0;

private:
  controller::cipher_engine* m_cipher = controller::cipher_engine::get_instance();
};