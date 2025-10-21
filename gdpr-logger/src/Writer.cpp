#include "Writer.hpp"
#include "Crypto.hpp"
#include "Compression.hpp"
#include <iostream>
#include <chrono>
#include <map>

Writer::Writer(BufferQueue &queue,
               std::shared_ptr<SegmentedStorage> storage,
               std::shared_ptr<TrustedCounter> trustedCounter,
               size_t batchSize,
               bool useEncryption,
               int compressionLevel)
    : m_queue(queue),
      m_storage(std::move(storage)),
      m_trustedCounter(std::move(trustedCounter)),
      m_batchSize(batchSize),
      m_useEncryption(useEncryption),
      m_compressionLevel(compressionLevel),
      m_consumerToken(queue.createConsumerToken()) {}

Writer::~Writer()
{
    stop();
}

void Writer::start()
{
    if (m_running.exchange(true))
    {
        return;
    }

    m_writerThread.reset(new std::thread(&Writer::processLogEntries, this));
}

void Writer::startGDPR()
{
    if (m_running.exchange(true))
    {
        return;
    }

    m_writerThread.reset(new std::thread(&Writer::processLogEntriesGDPR, this));
}

void Writer::stop()
{
    if (m_running.exchange(false))
    {
        if (m_writerThread && m_writerThread->joinable())
        {
            m_writerThread->join();
        }
    }
}

bool Writer::isRunning() const
{
    return m_running.load();
}

void Writer::processLogEntries()
{
    std::vector<QueueItem> batch;

    Crypto crypto;
    std::vector<uint8_t> encryptionKey(crypto.KEY_SIZE, 0x42); // dummy key
    std::vector<uint8_t> dummyIV(crypto.GCM_IV_SIZE, 0x24);    // dummy IV

    while (m_running)
    {
        size_t entriesDequeued = m_queue.tryDequeueBatch(batch, m_batchSize, m_consumerToken);
        if (entriesDequeued == 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        std::map<std::optional<std::string>, std::vector<LogEntry>> groupedEntries;
        for (auto &item : batch)
        {
            groupedEntries[item.targetFilename].emplace_back(std::move(item.entry));
        }

        for (auto &[targetFilename, entries] : groupedEntries)
        {
            std::vector<uint8_t> processedData = LogEntry::serializeBatch(std::move(entries));

            // Apply compression if enabled
            if (m_compressionLevel > 0)
            {
                processedData = Compression::compress(std::move(processedData), m_compressionLevel);
            }
            // Apply encryption if enabled
            if (m_useEncryption)
            {
                processedData = crypto.encrypt(std::move(processedData), encryptionKey, dummyIV);
            }

            if (targetFilename)
            {
                m_storage->writeToFile(*targetFilename, std::move(processedData));
            }
            else
            {
                m_storage->write(std::move(processedData));
            }
        }

        batch.clear();
    }
}

void Writer::processLogEntriesGDPR() {
    /*
     * Batch format:
     * [4 bytes]    Size of serialized LogEntry batch (N)
     * [4 bytes]    Trusted Counter (batchHeader uint_32_t)
     * [N bytes]    Serialized LogEntry batch data (encrypted/compressed if enabled)
     * [4 bytes]    HMAC/Tag (if encryption is used) including the Trusted counter and batch data
     */
    std::vector<QueueItem> batch;
    Crypto crypto;
    std::vector<uint8_t> encryptionKey(crypto.KEY_SIZE, 0x42);
    std::vector<uint8_t> dummyIV(crypto.GCM_IV_SIZE, 0x24);

    while (m_running) {
        size_t entriesDequeued = m_queue.tryDequeueBatch(batch, m_batchSize, m_consumerToken);
        if (entriesDequeued == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            continue;
        }

        std::map<std::optional<std::string>, std::vector<LogEntry>> groupedEntries;
        for (auto &item : batch) {
            groupedEntries[item.targetFilename].emplace_back(std::move(item.entry));
        }

        for (auto &[targetFilename, entries] : groupedEntries) {
            // Get the next counter value for this file/key
            std::string keyForCounter = targetFilename.value_or("default");
            uint32_t batchTrustedCounter = m_trustedCounter->getNextCounterForKey(keyForCounter);
            
            // Create batch data with counter header
            std::vector<uint8_t> batchHeader;
            batchHeader.resize(sizeof(uint32_t));
            std::memcpy(batchHeader.data(), &batchTrustedCounter, sizeof(batchTrustedCounter));

            std::vector<uint8_t> processedData = LogEntry::serializeBatchGDPR(std::move(entries));
            
            // Prepend counter to batch data
            processedData.insert(processedData.begin(), batchHeader.begin(), batchHeader.end());

            // Apply compression if enabled
            if (m_compressionLevel > 0) {
                processedData = Compression::compress(std::move(processedData), m_compressionLevel);
            }

            // Apply encryption if enabled
            if (m_useEncryption) {
                processedData = crypto.encrypt(std::move(processedData), encryptionKey, dummyIV);
            } else {
                // Add size header even for unencrypted data
                // in the encryption case, it's done in the encrypt() method
                size_t dataSize = processedData.size();
                std::vector<uint8_t> sizedData(sizeof(uint32_t) + dataSize);
                std::memcpy(sizedData.data(), &dataSize, sizeof(uint32_t));
                std::memcpy(sizedData.data() + sizeof(uint32_t), processedData.data(), dataSize);
                processedData = std::move(sizedData);
            }

            if (targetFilename) {
                m_storage->writeToFile(*targetFilename, std::move(processedData));
            } else {
                m_storage->write(std::move(processedData));
            }
        }

        batch.clear();
    }
}