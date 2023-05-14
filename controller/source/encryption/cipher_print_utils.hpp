#pragma once

#include <iostream>
#include <iomanip>
#include <string>
#include <array>
#include <vector>

/**
 * @file cipher_print_utils.hpp
 * @brief Header file containing utility functions for printing bytes and integers in hexadecimal format.
 *
 * This header file defines several functions for printing bytes and integers in hexadecimal format.
 * It provides flexibility to print bytes from different data types, such as arrays, vectors, and string-like objects.
 * Additionally, it includes a function specifically for printing the bytes of an integer value.
 *
 * The functions in this header are useful for debugging, logging, or any scenario where hexadecimal representation of data is needed.
 * The `printBytes` functions can handle various types of data buffers and print their contents in a readable hexadecimal format.
 * The `printIntBytes` function prints the bytes of an integer value, allowing examination of its internal representation.
 *
 * Example usage:
 *
 *    std::vector<unsigned char> data = {0x12, 0xAB, 0xCD, 0xEF};
 *    printBytes(data); // Output: 12 ABCDEF
 *
 *    int value = 0x12345678;
 *    printIntBytes(value); // Output: 12 34 56 78
 *
 * @note It is important to ensure that the input data is valid and properly formatted to avoid unexpected behavior.
 */

// NOLINTBEGIN

/**
 * Prints the bytes of a given data buffer in hexadecimal format.
 *
 * @tparam T     The type of the data buffer. It should have the member functions `data()` and `size()`.
 * @param data   The data buffer to print.
 */
template<typename T>
auto print_bytes(const T& data) -> void
{
  const auto bytes = reinterpret_cast<const unsigned char*>(data.data());
  size_t length = data.size();

  for (size_t i = 0; i < length; ++i) {
      std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(bytes[i]);
  }
  std::cout << std::dec << std::endl; // Reset stream formatting to decimal
}

/**
 * Prints the bytes of an integer value in hexadecimal format.
 *
 * @param value The integer value to print.
 */
inline auto print_int_bytes(int value) -> void
{
  const auto bytes = reinterpret_cast<const unsigned char*>(&value);
  size_t length = sizeof(value);

  for (size_t i = 0; i < length; ++i) {
      std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(bytes[i]);
  }
  std::cout << std::dec << std::endl; // Reset stream formatting to decimal
}

/**
 * Prints the bytes of a given data buffer in hexadecimal format.
 *
 * @param data   Pointer to the data buffer.
 * @param length Length of the data buffer.
 */
inline auto print_buffer_bytes(const unsigned char* data, size_t length) -> void
{
  for (size_t i = 0; i < length; ++i) {
      std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]);
  }
  std::cout << std::dec << std::endl; // Reset stream formatting to decimal
}

// NOLINTEND