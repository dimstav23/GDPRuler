#include "Logger.hpp"
#include "QueueItem.hpp"
#include <iostream>

// Initialize static members
std::unique_ptr<Logger> Logger::s_instance = nullptr;
std::mutex Logger::s_instanceMutex;

Logger &Logger::getInstance()
{
    std::lock_guard<std::mutex> lock(s_instanceMutex);
    if (s_instance == nullptr)
    {
        s_instance.reset(new Logger());
    }
    return *s_instance;
}

Logger::Logger()
    : m_logQueue(nullptr),
      m_appendTimeout(std::chrono::milliseconds::max()),
      m_initialized(false)
{
}

Logger::~Logger()
{
    if (m_initialized)
    {
        reset();
    }
}

bool Logger::initialize(std::shared_ptr<BufferQueue> queue,
                        std::chrono::milliseconds appendTimeout)
{
    if (m_initialized)
    {
        reportError("Logger already initialized");
        return false;
    }

    if (!queue)
    {
        reportError("Cannot initialize with a null queue");
        return false;
    }

    m_logQueue = std::move(queue);
    m_appendTimeout = appendTimeout;
    m_initialized = true;

    return true;
}

BufferQueue::ProducerToken Logger::createProducerToken()
{
    if (!m_initialized)
    {
        reportError("Logger not initialized");
        throw std::runtime_error("Logger not initialized");
    }

    return m_logQueue->createProducerToken();
}

bool Logger::append(LogEntry entry,
                    BufferQueue::ProducerToken &token,
                    const std::optional<std::string> &filename)
{
    if (!m_initialized)
    {
        reportError("Logger not initialized");
        return false;
    }

    QueueItem item{std::move(entry), filename};
    return m_logQueue->enqueueBlocking(std::move(item), token, m_appendTimeout);
}

bool Logger::appendBatch(std::vector<LogEntry> entries,
                         BufferQueue::ProducerToken &token,
                         const std::optional<std::string> &filename)
{
    if (!m_initialized)
    {
        reportError("Logger not initialized");
        return false;
    }

    if (entries.empty())
    {
        return true;
    }

    std::vector<QueueItem> batch;
    batch.reserve(entries.size());
    for (auto &entry : entries)
    {
        batch.emplace_back(std::move(entry), filename);
    }
    return m_logQueue->enqueueBatchBlocking(std::move(batch), token, m_appendTimeout);
}

bool Logger::reset()
{
    if (!m_initialized)
    {
        return false;
    }

    // Reset state
    m_initialized = false;
    m_logQueue.reset();

    return true;
}

void Logger::reportError(const std::string &message)
{
    std::cerr << "Logger Error: " << message << std::endl;
}