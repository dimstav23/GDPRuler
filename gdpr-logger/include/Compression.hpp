#ifndef COMPRESSION_HPP
#define COMPRESSION_HPP

#include "LogEntry.hpp"
#include <vector>
#include <cstdint>
#include <zlib.h>

class Compression
{
public:
    static std::vector<uint8_t> compress(std::vector<uint8_t> &&data, int level = Z_DEFAULT_COMPRESSION);

    static std::vector<uint8_t> decompress(std::vector<uint8_t> &&compressedData);
};

#endif