#pragma once

#include <iostream>
#include <string>
#include <optional>

#include "../encryption/encryptor.hpp"

using controller::encrypt;
using controller::decrypt;

class kv_client
{
public:
  /* kv_client interface signatures */
  auto get(const std::string& key) -> std::optional<std::string> {
    #ifndef ENCRYPTION_ENABLED
      // get the value directly w/o decryption
      return get_value(key);
    #else
      // get the value after decryption
      auto encrypted_value = get_value(key);
      if (!encrypted_value.has_value()) {
        return std::nullopt;
      }

      auto decrypt_result = decrypt(encrypted_value.value());
      if (decrypt_result.m_success) {
        return decrypt_result.m_plaintext;
      }
      std::cout << "Error in get: Decryption failed for value: " << encrypted_value.value() << std::endl;
      return std::nullopt;
    #endif
  }

  auto put(const std::string& key, const std::string& value) -> bool {
    #ifndef ENCRYPTION_ENABLED
      // put the pair directly w/o encryption
      return put_pair(key, value);
    #else
      // put the pair after encryption
      auto encrypt_result = encrypt(value);
      if (encrypt_result.m_success) {
        return put_pair(key, encrypt_result.m_ciphertext);
      }
      std::cout << "Error in put: Encryption failed for value: " << value << std::endl;
      return false;
    #endif
  }

  auto del(const std::string& key) -> bool {
    #ifndef ENCRYPTION_ENABLED
      // delete the pair directly w/o decryption
      return del_pair(key);
    #else
      // delete the pair directly w/o decryption
      return del_pair(key);
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
  virtual auto get_value(const std::string& key) -> std::optional<std::string> = 0;
  virtual auto put_pair(const std::string& key, const std::string& value) -> bool = 0;
  virtual auto del_pair(const std::string& key) -> bool = 0;
};