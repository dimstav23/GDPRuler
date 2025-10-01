#pragma once

#include <iostream>
#include <string>
#include <optional>

#include "../encryption/cipher_engine.hpp"

class kv_client
{
public:
  /* kv_client interface signatures */
  inline auto gdpr_get(std::string_view key) -> std::optional<std::string> {
    #ifndef ENCRYPTION_ENABLED
      // get the value directly w/o decryption
      return std::move(get(key));
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
  inline auto gdpr_put(std::string_view key, std::string_view value) -> bool {
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

  inline auto gdpr_del(std::string_view key) -> bool {
    // delete the pair directly w/o decryption
    return del(key);
  }

  auto gdpr_getm(std::string_view key_prefix) -> std::vector<std::string> {
    #ifndef ENCRYPTION_ENABLED
      // get the values directly w/o decryption
      return getm(key_prefix);
    #else
      // get the values after decryption
      auto encrypted_values = getm(key_prefix);
      std::vector<std::string> decrypted_values;
      decrypted_values.reserve(encrypted_values.size());
      
      for (auto& encrypted_value : encrypted_values) {
        auto decrypt_result = m_cipher->decrypt(encrypted_value, cipher_key_type::db_key);
        if (decrypt_result.m_success) {
          decrypted_values.emplace_back(std::move(decrypt_result.m_plaintext));
        } else {
          std::cerr << "Error in getm: Decryption failed for value: " 
                    << encrypted_value << std::endl;
        }
      }
      
      return decrypted_values;
    #endif
  }

  auto gdpr_get_prefix_kv_pairs(std::string_view key_prefix) -> std::vector<std::pair<std::string, std::string>> {
    #ifndef ENCRYPTION_ENABLED
      return get_prefix_kv_pairs(key_prefix);
    #else
      auto encrypted_pairs = get_prefix_kv_pairs(key_prefix);
      std::vector<std::pair<std::string, std::string>> decrypted_pairs;
      decrypted_pairs.reserve(encrypted_pairs.size());
      
      for (auto& [key, encrypted_value] : encrypted_pairs) {
        auto decrypt_result = m_cipher->decrypt(encrypted_value, cipher_key_type::db_key);
        if (decrypt_result.m_success) {
          decrypted_pairs.emplace_back(std::move(key), std::move(decrypt_result.m_plaintext));
        } else {
          std::cerr << "Error in get_prefix_kv_pairs: Decryption failed for key: " << key << std::endl;
        }
      }
      
      return decrypted_pairs;
    #endif
  }
  
  auto gdpr_putm(const std::vector<std::pair<std::string, std::string>>& key_value_pairs) -> std::vector<bool> {
    #ifndef ENCRYPTION_ENABLED
    // Direct bulk update without encryption
    return putm(key_value_pairs);
    #else
    // Encrypt all values before bulk update
    std::vector<std::pair<std::string, std::string>> encrypted_pairs;
    encrypted_pairs.reserve(key_value_pairs.size());
    std::vector<bool> results;
    results.reserve(key_value_pairs.size());
    
    for (const auto& [key, value] : key_value_pairs) {
      auto encrypt_result = m_cipher->encrypt(value, cipher_key_type::db_key);
      if (encrypt_result.m_success) {
        encrypted_pairs.emplace_back(key, encrypt_result.m_ciphertext);
      } else {
        std::cerr << "Error in putm_bulk: Encryption failed for key: " << key << std::endl;
        // Still try to process other pairs, but mark this as failed
      }
    }
    
    if (encrypted_pairs.empty()) {
      results.resize(key_value_pairs.size(), false);
      return results;
    }
    
    return putm(encrypted_pairs);
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
  virtual auto get(std::string_view key) -> std::optional<std::string> = 0;
  virtual auto put(std::string_view key, std::string_view value) -> bool = 0;
  virtual auto del(std::string_view key) -> bool = 0;
  /* GDPR queries */
  virtual auto getm(std::string_view key_prefix) -> std::vector<std::string> = 0;
  // virtual auto putm(std::string_view key_prefix, std::string_view value) -> bool = 0;
  virtual auto putm(const std::vector<std::pair<std::string, std::string>>& key_value_pairs) -> std::vector<bool> = 0;
  virtual auto get_prefix_kv_pairs(std::string_view key_prefix) -> std::vector<std::pair<std::string, std::string>> = 0;

private:
  controller::cipher_engine* m_cipher = controller::cipher_engine::get_instance();
};