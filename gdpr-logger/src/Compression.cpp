#include "Compression.hpp"
#include <stdexcept>
#include <cstring>
#include <iostream>

// Helper function to compress raw data using zlib
std::vector<uint8_t> Compression::compress(std::vector<uint8_t> &&data, int level)
{
    if (data.empty())
    {
        return std::vector<uint8_t>();
    }

    z_stream zs;
    std::memset(&zs, 0, sizeof(zs));

    // Use the provided compression level instead of hardcoded Z_BEST_COMPRESSION
    if (deflateInit(&zs, level) != Z_OK)
    {
        throw std::runtime_error("Failed to initialize zlib deflate");
    }

    zs.next_in = const_cast<Bytef *>(data.data());
    zs.avail_in = data.size();

    int ret;
    char outbuffer[32768];
    std::vector<uint8_t> compressedData;

    // Compress data in chunks
    do
    {
        zs.next_out = reinterpret_cast<Bytef *>(outbuffer);
        zs.avail_out = sizeof(outbuffer);

        ret = deflate(&zs, Z_FINISH);

        if (compressedData.size() < zs.total_out)
        {
            compressedData.insert(compressedData.end(),
                                  outbuffer,
                                  outbuffer + (zs.total_out - compressedData.size()));
        }
    } while (ret == Z_OK);

    deflateEnd(&zs);

    if (ret != Z_STREAM_END)
    {
        throw std::runtime_error("Exception during zlib compression");
    }

    return compressedData;
}

// Helper function to decompress raw data using zlib
std::vector<uint8_t> Compression::decompress(std::vector<uint8_t> &&compressedData)
{
    if (compressedData.empty())
    {
        return std::vector<uint8_t>();
    }

    z_stream zs;
    std::memset(&zs, 0, sizeof(zs));

    if (inflateInit(&zs) != Z_OK)
    {
        throw std::runtime_error("Failed to initialize zlib inflate");
    }

    zs.next_in = const_cast<Bytef *>(compressedData.data());
    zs.avail_in = compressedData.size();

    int ret;
    char outbuffer[32768];
    std::vector<uint8_t> decompressedData;

    // Decompress data in chunks
    do
    {
        zs.next_out = reinterpret_cast<Bytef *>(outbuffer);
        zs.avail_out = sizeof(outbuffer);

        ret = inflate(&zs, Z_NO_FLUSH);

        if (ret == Z_NEED_DICT || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR)
        {
            inflateEnd(&zs);
            throw std::runtime_error("Exception during zlib decompression");
        }

        if (decompressedData.size() < zs.total_out)
        {
            decompressedData.insert(decompressedData.end(),
                                    outbuffer,
                                    outbuffer + (zs.total_out - decompressedData.size()));
        }
    } while (ret == Z_OK);

    inflateEnd(&zs);

    if (ret != Z_STREAM_END)
    {
        throw std::runtime_error("Exception during zlib decompression");
    }

    return decompressedData;
}