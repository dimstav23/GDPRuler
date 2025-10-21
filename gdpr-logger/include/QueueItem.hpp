#ifndef QUEUE_ITEM_HPP
#define QUEUE_ITEM_HPP

#include "LogEntry.hpp"
#include <optional>
#include <string>

struct QueueItem
{
    LogEntry entry;
    std::optional<std::string> targetFilename = std::nullopt;

    QueueItem() = default;
    QueueItem(LogEntry &&logEntry)
        : entry(std::move(logEntry)), targetFilename(std::nullopt) {}
    QueueItem(LogEntry &&logEntry, const std::optional<std::string> &filename)
        : entry(std::move(logEntry)), targetFilename(filename) {}

    QueueItem(const QueueItem &) = default;
    QueueItem(QueueItem &&) = default;
    QueueItem &operator=(const QueueItem &) = default;
    QueueItem &operator=(QueueItem &&) = default;
};

#endif