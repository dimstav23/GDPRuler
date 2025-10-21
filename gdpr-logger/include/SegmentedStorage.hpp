#ifndef SEGMENTED_STORAGE_HPP
#define SEGMENTED_STORAGE_HPP

#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <filesystem>
#include <cstdint>
#include <unordered_map>
#include <fcntl.h>  // for open flags
#include <unistd.h> // for close, pwrite, fsync
#include <chrono>
#include <thread>
#include <stdexcept>
#include <list> // For LRU cache
#include <cerrno>
#include <cstring>
#include <iostream>

class SegmentedStorage
{
public:
    SegmentedStorage(const std::string &basePath,
                     const std::string &baseFilename,
                     size_t maxSegmentSize = 100 * 1024 * 1024, // 100 MB default
                     size_t maxAttempts = 5,
                     std::chrono::milliseconds baseRetryDelay = std::chrono::milliseconds(1),
                     size_t maxOpenFiles = 512);

    ~SegmentedStorage();

    size_t write(std::vector<uint8_t> &&data);
    size_t writeToFile(const std::string &filename, std::vector<uint8_t> &&data);
    void flush();
    
    std::vector<std::string> getSegmentFiles() const;
    std::vector<std::string> getSegmentFilesForKey(const std::string& key) const;
    std::string getBasePath() const { return m_basePath; }

private:
    std::string m_basePath;
    std::string m_baseFilename;
    size_t m_maxSegmentSize;
    size_t m_maxAttempts;
    std::chrono::milliseconds m_baseRetryDelay;

    struct CacheEntry
    {
        int fd{-1};
        std::atomic<size_t> segmentIndex{0};
        std::atomic<size_t> currentOffset{0};
        std::string currentSegmentPath;
        mutable std::shared_mutex fileMutex; // shared for writes, exclusive for rotate/flush

        // Deletion detection metadata
        struct stat originalStat;
        bool hasOriginalStat = false;
    };

    // Unified LRU Cache for both file descriptors and segment information
    class LRUCache
    {
    public:
        LRUCache(size_t capacity, SegmentedStorage *parent) : m_capacity(capacity), m_parent(parent) {}

        std::shared_ptr<CacheEntry> get(const std::string &filename);
        void flush(const std::string &filename);
        void flushAll();
        void closeAll();

    private:
        size_t m_capacity;
        SegmentedStorage *m_parent;

        // LRU list of filenames
        std::list<std::string> m_lruList;
        // Map from filename to cache entry and iterator in LRU list
        struct CacheData
        {
            std::shared_ptr<CacheEntry> entry;
            std::list<std::string>::iterator lruIt;
        };
        std::unordered_map<std::string, CacheData> m_cache;
        mutable std::mutex m_mutex; // Protects m_lruList and m_cache

        void evictLRU();
        std::shared_ptr<CacheEntry> reconstructState(const std::string &filename);
    };

    LRUCache m_cache;

    std::string rotateSegment(const std::string &filename, std::shared_ptr<CacheEntry> entry);
    std::string generateSegmentPath(const std::string &filename, size_t segmentIndex) const;
    size_t getFileSize(const std::string &path) const;
    bool isFileDeleted(int fd) const;
    size_t findLatestSegmentIndex(const std::string &filename) const;

    // Retry helpers use member-configured parameters
    template <typename Func>
    auto retryWithBackoff(Func &&f)
    {
        for (size_t attempt = 1;; ++attempt)
        {
            try
            {
                return f();
            }
            catch (const std::runtime_error &)
            {
                if (attempt >= m_maxAttempts)
                    throw;
                auto delay = m_baseRetryDelay * (1 << (attempt - 1));
                std::this_thread::sleep_for(delay);
            }
        }
    }

    int openWithRetry(const char *path, int flags, mode_t mode)
    {
        return retryWithBackoff([&]()
                                {
            int fd = ::open(path, flags, mode);
            if (fd < 0) throw std::runtime_error("open failed");
            return fd; });
    }

    size_t pwriteFull(int fd, const uint8_t *buf, size_t count, off_t offset)
    {
        size_t total = 0;
        while (total < count)
        {
            ssize_t written = ::pwrite(fd, buf + total, count - total, offset + total);
            if (written < 0)
            {
                int err = errno;
                std::cerr << "pwrite failed:" << std::endl;
                std::cerr << "  Error: " << strerror(err) << " (errno=" << err << ")" << std::endl;
                std::cerr << "  File descriptor: " << fd << std::endl;
                std::cerr << "  Buffer address: " << static_cast<const void*>(buf + total) << std::endl;
                std::cerr << "  Bytes requested: " << (count - total) << std::endl;
                std::cerr << "  File offset: " << (offset + total) << std::endl;
                std::cerr << "  Total written so far: " << total << std::endl;
                
                if (err != EINTR) {
                    throw std::runtime_error(std::string("pwrite failed: ") + strerror(err));
                }
            }
            total += written;
        }
        return total;
    }

    void fsyncRetry(int fd)
    {
        retryWithBackoff([&]()
                         {
            if (::fsync(fd) < 0) throw std::runtime_error("fsync failed");
            return 0; });
    }
};

#endif