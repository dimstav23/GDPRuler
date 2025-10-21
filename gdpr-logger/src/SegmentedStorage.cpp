#include "SegmentedStorage.hpp"
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <sys/stat.h>

SegmentedStorage::SegmentedStorage(const std::string &basePath,
                                   const std::string &baseFilename,
                                   size_t maxSegmentSize,
                                   size_t maxAttempts,
                                   std::chrono::milliseconds baseRetryDelay,
                                   size_t maxOpenFiles)
    : m_basePath(basePath),
      m_baseFilename(baseFilename),
      m_maxSegmentSize(maxSegmentSize),
      m_maxAttempts(maxAttempts),
      m_baseRetryDelay(baseRetryDelay),
      m_cache(maxOpenFiles, this)
{
    std::filesystem::create_directories(m_basePath);
}

SegmentedStorage::~SegmentedStorage()
{
    m_cache.closeAll();
}

// LRUCache methods
std::shared_ptr<SegmentedStorage::CacheEntry> SegmentedStorage::LRUCache::get(const std::string &filename)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_cache.find(filename);
    if (it != m_cache.end())
    {
        // Found in cache, move to front (most recently used)
        m_lruList.erase(it->second.lruIt);
        m_lruList.push_front(filename);
        it->second.lruIt = m_lruList.begin();
        return it->second.entry;
    }

    // Not in cache, need to reconstruct state
    auto entry = reconstructState(filename);

    // Check if we need to evict
    if (m_cache.size() >= m_capacity)
    {
        evictLRU();
    }

    // Add to cache
    m_lruList.push_front(filename);
    m_cache[filename] = {entry, m_lruList.begin()};

    return entry;
}

void SegmentedStorage::LRUCache::evictLRU()
{
    // Called with m_mutex already locked
    if (m_lruList.empty())
        return;

    const std::string &lru_filename = m_lruList.back();
    auto it = m_cache.find(lru_filename);
    if (it != m_cache.end())
    {
        // Close the file descriptor if it's open
        if (it->second.entry->fd >= 0)
        {
            m_parent->fsyncRetry(it->second.entry->fd);
            ::close(it->second.entry->fd);
        }
        m_cache.erase(it);
    }
    m_lruList.pop_back();
}

std::shared_ptr<SegmentedStorage::CacheEntry> SegmentedStorage::LRUCache::reconstructState(const std::string &filename)
{
    // Called with m_mutex already locked
    auto entry = std::make_shared<CacheEntry>();

    // Find the latest segment index for this filename
    size_t latestIndex = m_parent->findLatestSegmentIndex(filename);
    entry->segmentIndex.store(latestIndex, std::memory_order_release);

    // Generate the path for the current segment
    std::string segmentPath = m_parent->generateSegmentPath(filename, latestIndex);
    entry->currentSegmentPath = segmentPath;

    // Open the file and get its current size
    entry->fd = m_parent->openWithRetry(segmentPath.c_str(), O_CREAT | O_RDWR | O_APPEND, 0644);

    // Store original file metadata
    if (entry->fd >= 0 && ::fstat(entry->fd, &entry->originalStat) == 0) {
        entry->hasOriginalStat = true;
    }

    // Get the current file size to set as the offset
    size_t fileSize = m_parent->getFileSize(segmentPath);
    entry->currentOffset.store(fileSize, std::memory_order_release);

    return entry;
}

void SegmentedStorage::LRUCache::flush(const std::string &filename)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_cache.find(filename);
    if (it != m_cache.end() && it->second.entry->fd >= 0)
    {
        m_parent->fsyncRetry(it->second.entry->fd);
    }
}

void SegmentedStorage::LRUCache::flushAll()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto &pair : m_cache)
    {
        if (pair.second.entry->fd >= 0)
        {
            m_parent->fsyncRetry(pair.second.entry->fd);
        }
    }
}

void SegmentedStorage::LRUCache::closeAll()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto &pair : m_cache)
    {
        if (pair.second.entry->fd >= 0)
        {
            m_parent->fsyncRetry(pair.second.entry->fd);
            ::close(pair.second.entry->fd);
        }
    }
    m_cache.clear();
    m_lruList.clear();
}

size_t SegmentedStorage::findLatestSegmentIndex(const std::string &filename) const
{
    size_t maxIndex = 0;
    std::string pattern = filename + "_";

    try
    {
        for (const auto &entry : std::filesystem::directory_iterator(m_basePath))
        {
            if (entry.is_regular_file())
            {
                std::string name = entry.path().filename().string();
                if (name.find(pattern) == 0)
                {
                    // Extract the index from filename format: filename_YYYYMMDD_HHMMSS_NNNNNN.log
                    size_t lastUnderscore = name.find_last_of('_');
                    if (lastUnderscore != std::string::npos)
                    {
                        size_t dotPos = name.find('.', lastUnderscore);
                        if (dotPos != std::string::npos)
                        {
                            std::string indexStr = name.substr(lastUnderscore + 1, dotPos - lastUnderscore - 1);
                            try
                            {
                                size_t index = std::stoull(indexStr);
                                maxIndex = std::max(maxIndex, index);
                            }
                            catch (...)
                            {
                                // Ignore files that don't match the expected format
                            }
                        }
                    }
                }
            }
        }
    }
    catch (const std::filesystem::filesystem_error &)
    {
        // If directory doesn't exist or other filesystem error, return 0
    }

    return maxIndex;
}

size_t SegmentedStorage::getFileSize(const std::string &path) const
{
    struct stat st;
    if (::stat(path.c_str(), &st) == 0)
    {
        return static_cast<size_t>(st.st_size);
    }
    return 0;
}

bool SegmentedStorage::isFileDeleted(int fd) const
{
    struct stat st;
    if (::fstat(fd, &st) != 0) {
        return true; // Consider invalid fd as "deleted"
    }

    // If nlink is 0, the file has been unlinked from all directory entries
    return st.st_nlink == 0;
}

size_t SegmentedStorage::write(std::vector<uint8_t> &&data)
{
    return writeToFile(m_baseFilename, std::move(data));
}

size_t SegmentedStorage::writeToFile(const std::string &filename, std::vector<uint8_t> &&data)
{
    size_t size = data.size();
    if (size == 0)
        return 0;

    std::shared_ptr<CacheEntry> entry = m_cache.get(filename);
    size_t writeOffset;

    // This loop handles race conditions around rotation
    while (true)
    {
        // First check if we need to rotate WITHOUT reserving space
        size_t currentOffset = entry->currentOffset.load(std::memory_order_acquire);
        if (currentOffset + size > m_maxSegmentSize)
        {
            std::unique_lock<std::shared_mutex> rotLock(entry->fileMutex);
            // Double-check if rotation is still needed
            if (entry->currentOffset.load(std::memory_order_acquire) + size > m_maxSegmentSize)
            {
                rotateSegment(filename, entry);
                // After rotation, entry has been updated with new fd and path
                continue;
            }
        }

        // Now safely reserve space
        writeOffset = entry->currentOffset.fetch_add(size, std::memory_order_acq_rel);

        // Double-check we didn't cross the boundary after reservation
        if (writeOffset + size > m_maxSegmentSize)
        {
            // Another thread increased the offset past our threshold, try again
            continue;
        }

        // We have a valid offset and can proceed with the write
        break;
    }

    // Write under shared lock to prevent racing with rotate/close
    {
        std::shared_lock<std::shared_mutex> writeLock(entry->fileMutex);

        // Check if file was deleted before writing
        if (entry->fd < 0 || isFileDeleted(entry->fd)) {
            // File was deleted, need to recreate
            writeLock.unlock();

            // Get exclusive lock and recreate file
            std::unique_lock<std::shared_mutex> exclusiveLock(entry->fileMutex);
            if (entry->fd >= 0) {
                ::close(entry->fd);
            }
            entry->fd = openWithRetry(entry->currentSegmentPath.c_str(),
                                      O_CREAT | O_RDWR | O_APPEND, 0644);

            exclusiveLock.unlock();
            return writeToFile(filename, std::move(data)); // Retry
        }

        pwriteFull(entry->fd, data.data(), size, static_cast<off_t>(writeOffset));
    }

    return size;
}

void SegmentedStorage::flush()
{
    m_cache.flushAll();
}

std::string SegmentedStorage::rotateSegment(const std::string &filename, std::shared_ptr<CacheEntry> entry)
{
    // exclusive lock assumed by the caller (writeToFile)

    // Close the old file descriptor
    if (entry->fd >= 0)
    {
        fsyncRetry(entry->fd);
        ::close(entry->fd);
        entry->fd = -1;
    }

    size_t newIndex = entry->segmentIndex.fetch_add(1, std::memory_order_acq_rel) + 1;
    entry->currentOffset.store(0, std::memory_order_release);
    std::string newPath = generateSegmentPath(filename, newIndex);

    // Update the entry's path and open the new file
    entry->currentSegmentPath = newPath;
    entry->fd = openWithRetry(newPath.c_str(), O_CREAT | O_RDWR | O_APPEND, 0644);

    return newPath;
}

std::string SegmentedStorage::generateSegmentPath(const std::string &filename, size_t segmentIndex) const
{
    std::stringstream ss;
    ss << m_basePath << "/";
    ss << filename << "_";
    ss << std::setw(6) << std::setfill('0') << segmentIndex << ".log";
    return ss.str();
}

std::vector<std::string> SegmentedStorage::getSegmentFiles() const
{
  std::vector<std::string> segmentFiles;
  
  try {
    if (!std::filesystem::exists(m_basePath)) {
      std::cerr << "SegmentedStorage: Base path does not exist: " << m_basePath << std::endl;
      return segmentFiles;
    }

    for (const auto& entry : std::filesystem::directory_iterator(m_basePath)) {
      if (entry.is_regular_file()) {
        std::string filename = entry.path().filename().string();
        // Check if it's a log file (contains .log extension)
        if (filename.find(".log") != std::string::npos) {
          segmentFiles.push_back(entry.path().string());
        }
      }
    }

    // Sort files by name to ensure chronological order
    std::sort(segmentFiles.begin(), segmentFiles.end());
      
  } catch (const std::filesystem::filesystem_error& e) {
    std::cerr << "SegmentedStorage: Error listing files: " << e.what() << std::endl;
  }

  return segmentFiles;
}

std::vector<std::string> SegmentedStorage::getSegmentFilesForKey(const std::string& key) const
{
  std::vector<std::string> keyFiles;
  auto allFiles = getSegmentFiles();
  
  for (const auto& file : allFiles) {
    std::filesystem::path filePath(file);
    std::string filename = filePath.stem().string(); // Without extension
    
    // Check if file belongs to this key
    if (filename == key || 
      (filename.length() > key.length() + 1 && 
        filename.substr(0, key.length()) == key && 
        filename[key.length()] == '_')) {
      keyFiles.push_back(file);
    }
  }
  
  std::sort(keyFiles.begin(), keyFiles.end());
  return keyFiles;
}