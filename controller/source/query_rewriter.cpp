#include "query_rewriter.hpp"
#include <iostream>

namespace controller {

query_rewriter::query_rewriter()
{
}

/* Constructor for put query operation rewriter in case of the first INSERTION of a KV pair */
/* create the new value based on the query arguments and the default policy */
query_rewriter::query_rewriter(const query &query_args, 
                               const default_policy &def_policy,
                               std::string_view new_query_value)
{
  // Create header
  metadata_header header;
  header.flags = (def_policy.encryption() ? 1 : 0) | (query_args.monitor().value_or(def_policy.monitor()) ? 2 : 0);
  header.expiration_time = query_args.expiration().value_or(def_policy.expiration());

  // Pre-calculate the size of the final string 
  size_t total_size =  /* metadata header */    sizeof(metadata_header) + /* user key */    header.user_bytes     +
                        /* purpose */           header.purpose_bytes    + /* objection */   header.purpose_bytes  +
                        /* origin */            header.origin_bytes     + /* share */       header.user_bytes     + 
                        /* new value */         new_query_value.size();

  // Reserve space for the entire string
  m_new_value.reserve(total_size);

  // Append header
  append_header(header);
    
  // Append bitsets as compact bytes
  append_bitset(query_args.user_key().value_or(def_policy.user_key()));
  append_bitset(query_args.purpose().value_or(def_policy.purpose()));
  append_bitset(query_args.objection().value_or(def_policy.objection()));
  append_bitset(query_args.origin().value_or(def_policy.origin()));
  append_bitset(query_args.share().value_or(def_policy.share()));

  // Append query value
  m_new_value.append(new_query_value);
}

/* Constructor for put query operation rewriter in case of an UPDATE of a value */
// To suppress bugprone-easily-swappable-parameters warning from clang-tidy
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
query_rewriter::query_rewriter(std::string_view existing_metadata, std::string_view new_query_value)
{
  // Decode the existing binary format
  size_t offset = 0;
  metadata_header header = decode_header(existing_metadata, offset);

  // Calculate where the old query value starts
  size_t metadata_size =  /* metadata header */   sizeof(metadata_header) + /* user key */    header.user_bytes     +
                          /* purpose */           header.purpose_bytes    + /* objection */   header.purpose_bytes  +
                          /* origin */            header.origin_bytes     + /* share */       header.user_bytes;
  
  // Copy metadata prefix and append new query value
  m_new_value.reserve(metadata_size + new_query_value.size());
  m_new_value.append(existing_metadata.substr(0, metadata_size));
  m_new_value.append(new_query_value);
}

/* Constructor for the PUTM & put_only_metadata operation in case of an UPDATE of the metadata */
query_rewriter::query_rewriter(const query &query_args,
                               std::string_view existing_value)
{
  /* create the new metadata fields based on the query arguments - the rest are left intact */
  // Decode existing data
  size_t offset = 0;
  metadata_header header = decode_header(existing_value, offset);

  // Update header fields if needed
  if (query_args.expiration().has_value()) {
    header.expiration_time = get_expiration_time(query_args.expiration().value());
  }
  if (query_args.monitor().has_value()) {
    header.flags = (header.flags & 1) | (query_args.monitor().value() ? 2 : 0);
  }

  // Extract existing bitsets
  std::bitset<num_users> user_key;
  std::bitset<num_purposes> purpose;
  std::bitset<num_purposes> objection;
  std::bitset<num_origins> origin;
  std::bitset<num_users> share;
  
  size_t bitset_offset = offset;  // Save the current offset for bitsets

  // Update with new values if provided
  // User key
  if (query_args.user_key().has_value()) {
    user_key = query_args.user_key().value();
  } else {
    user_key = convert_to_bitset<num_users>(existing_value, bitset_offset, header.user_bytes);
  }
  bitset_offset += header.user_bytes;
  // Purpose
  if (query_args.purpose().has_value()) {
    purpose = query_args.purpose().value();
  } else {
    purpose = convert_to_bitset<num_purposes>(existing_value, bitset_offset, header.purpose_bytes);
  }
  bitset_offset += header.purpose_bytes;
  // Objection
  if (query_args.objection().has_value()) {
    objection = query_args.objection().value();
  } else {
    objection = convert_to_bitset<num_purposes>(existing_value, bitset_offset, header.purpose_bytes);
  }
  bitset_offset += header.purpose_bytes;
  // Origin
  if (query_args.origin().has_value()) {
    origin = query_args.origin().value();
  } else {
    origin = convert_to_bitset<num_origins>(existing_value, bitset_offset, header.origin_bytes);
  }
  bitset_offset += header.origin_bytes;
  // Share
  if (query_args.share().has_value()) {
    share = query_args.share().value();
  } else {
    share = convert_to_bitset<num_users>(existing_value, bitset_offset, header.user_bytes);
  }
  bitset_offset += header.user_bytes;

  // Extract existing data value (everything after the metadata)
  std::string_view existing_data_value;
  if (bitset_offset < existing_value.size()) {
    existing_data_value = existing_value.substr(bitset_offset);
  }

  size_t total_size =  /* metadata header */  sizeof(metadata_header) + /* user key */    header.user_bytes     +
                      /* purpose */           header.purpose_bytes    + /* objection */   header.purpose_bytes  +
                      /* origin */            header.origin_bytes     + /* share */       header.user_bytes     + 
                      /* new value */         existing_data_value.size();

  m_new_value.reserve(total_size);
  
  // Build new value with updated metadata and existing data
  append_header(header);
  append_bitset(user_key);
  append_bitset(purpose);
  append_bitset(objection);
  append_bitset(origin);
  append_bitset(share);
  m_new_value.append(existing_data_value); // Preserve existing data
}

/* Constructor for PUTC query operation rewriter */
/* create the new metadata fields based on the query arguments - the rest are left intact */
/* in case of PUTC, also update the value based on the new_query_value provided parameter */
query_rewriter::query_rewriter(const query &query_args,
                               std::string_view existing_metadata,
                               std::optional<std::string_view> new_query_value = std::nullopt)
{
  /* create the new metadata fields based on the query arguments - the rest are left intact */
  // Decode existing data
  size_t offset = 0;
  metadata_header header = decode_header(existing_metadata, offset);

  // Update header fields if needed
  if (query_args.expiration().has_value()) {
    header.expiration_time = get_expiration_time(query_args.expiration().value());
  }
  if (query_args.monitor().has_value()) {
    header.flags = (header.flags & 1) | (query_args.monitor().value() ? 2 : 0);
  }

  // Extract existing bitsets
  std::bitset<num_users> user_key;
  std::bitset<num_purposes> purpose;
  std::bitset<num_purposes> objection;
  std::bitset<num_origins> origin;
  std::bitset<num_users> share;
  
  size_t bitset_offset = offset;  // Save the current offset for bitsets

  // Update with new values if provided
  // User key
  if (query_args.user_key().has_value()) {
    user_key = query_args.user_key().value();
  } else {
    user_key = convert_to_bitset<num_users>(existing_metadata, bitset_offset, header.user_bytes);
  }
  bitset_offset += header.user_bytes;
  // Purpose
  if (query_args.purpose().has_value()) {
    purpose = query_args.purpose().value();
  } else {
    purpose = convert_to_bitset<num_purposes>(existing_metadata, bitset_offset, header.purpose_bytes);
  }
  bitset_offset += header.purpose_bytes;
  // Objection
  if (query_args.objection().has_value()) {
    objection = query_args.objection().value();
  } else {
    objection = convert_to_bitset<num_purposes>(existing_metadata, bitset_offset, header.purpose_bytes);
  }
  bitset_offset += header.purpose_bytes;
  // Origin
  if (query_args.origin().has_value()) {
    origin = query_args.origin().value();
  } else {
    origin = convert_to_bitset<num_origins>(existing_metadata, bitset_offset, header.origin_bytes);
  }
  bitset_offset += header.origin_bytes;
  // Share
  if (query_args.share().has_value()) {
    share = query_args.share().value();
  } else {
    share = convert_to_bitset<num_users>(existing_metadata, bitset_offset, header.user_bytes);
  }

  // Extract remaining query value
  std::string_view query_value = "";
  if (new_query_value.has_value()) {
    query_value = new_query_value.value();
  }

  size_t total_size =  /* metadata header */  sizeof(metadata_header) + /* user key */    header.user_bytes     +
                      /* purpose */           header.purpose_bytes    + /* objection */   header.purpose_bytes  +
                      /* origin */            header.origin_bytes     + /* share */       header.user_bytes     + 
                      /* new value */         query_value.size();

  m_new_value.reserve(total_size);
  
  append_header(header);
  append_bitset(user_key);
  append_bitset(purpose);
  append_bitset(objection);
  append_bitset(origin);
  append_bitset(share);
  m_new_value.append(query_value);
}

template<size_t N>
auto query_rewriter::append_bitset(const std::bitset<N>& bits) -> void {
  constexpr size_t num_bytes = (N + 7) / 8;
  
  if constexpr (N <= 64) {
    // For small bitsets, use direct conversion
    uint64_t value = bits.to_ullong();
    const char* value_ptr = reinterpret_cast<const char*>(&value);
    m_new_value.append(value_ptr, num_bytes);
  } else {
    // For larger bitsets, use word-level operations
    constexpr size_t words_needed = (num_bytes + 7) / 8;
    
    for (size_t word_idx = 0; word_idx < words_needed; ++word_idx) {
      uint64_t word = 0;
      size_t bit_offset = word_idx * 64;
      
      // Extract 64 bits at a time
      for (size_t bit = 0; bit < 64 && bit_offset + bit < N; ++bit) {
        if (bits[bit_offset + bit]) {
          word |= (1ULL << bit);
        }
      }
      
      // Append the word as bytes
      size_t bytes_in_word = std::min(size_t(8), num_bytes - word_idx * 8);
      const char* word_ptr = reinterpret_cast<const char*>(&word);
      m_new_value.append(word_ptr, bytes_in_word);
    }
  }
}

auto query_rewriter::append_header(const metadata_header& header) -> void {
  m_new_value.append(reinterpret_cast<const char*>(&header), sizeof(header));
}

auto query_rewriter::decode_header(std::string_view new_value, size_t& offset) const -> metadata_header {
  metadata_header header = *reinterpret_cast<const metadata_header*>(new_value.data() + offset);
  offset += sizeof(metadata_header);
  return header;
}

auto query_rewriter::new_value() && -> std::string {
  return std::move(m_new_value);  // Transfer ownership
}

} // namespace controller