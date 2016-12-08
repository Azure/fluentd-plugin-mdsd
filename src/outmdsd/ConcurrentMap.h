#pragma once
#ifndef __ENDPOINTLOG_CONCURRENTMAP_H__
#define __ENDPOINTLOG_CONCURRENTMAP_H__

#include <unordered_map>
#include <string>
#include <memory>
#include <mutex>
#include <vector>
#include <functional>

#include "LogItemPtr.h"

namespace EndpointLog {

/// This class implements thread-safe add/remove items from a hash cache.
/// It is not designed to be a generic map class. It uses specific
/// key/value pairs and only implements necessary APIs used in this project.
class ConcurrentMap {
public:
    ConcurrentMap() = default;
    ~ConcurrentMap() = default;

    ConcurrentMap(const ConcurrentMap& other);
    ConcurrentMap(ConcurrentMap&& other);

    ConcurrentMap & operator=(const ConcurrentMap& other);
    ConcurrentMap & operator=(ConcurrentMap&& other);

    /// Add new key, value pair
    /// If key exists, old entry will be replaced.
    void Add(const std::string & key, LogItemPtr value);

    /// Erase an item with given key
    /// Return 1 if erased, 0 if nothing is erased.
    size_t Erase(const std::string & key);

    /// Erase a list of items given their keys
    /// Return number of items erased.
    size_t Erase(const std::vector<std::string>& keylist);

    /// Return all the keys such that fn(value) == true.
    std::vector<std::string> FilterEach(const std::function<bool(LogItemPtr)>& fn);

    /// Apply fn on each value of the cache.
    void ForEach(const std::function<void(LogItemPtr)>& fn);

    /// Apply fn on each value of the cache without locking the mutex.
    /// This is not designed to be thread-safe.
    void ForEachUnsafe(const std::function<void(LogItemPtr)>& fn);

    LogItemPtr Get(const std::string & key);
    size_t Size() const;

private:
    std::unordered_map<std::string, LogItemPtr> m_cache;
    mutable std::mutex m_cacheMutex;
};

} // namespace
#endif // __ENDPOINTLOG_CONCURRENTMAP_H__
