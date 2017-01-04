#pragma once
#ifndef __ENDPOINTLOG_CONCURRENTMAP_H__
#define __ENDPOINTLOG_CONCURRENTMAP_H__

#include <unordered_map>
#include <string>
#include <mutex>
#include <vector>
#include <functional>
#include <algorithm>

namespace EndpointLog {

/// This class implements thread-safe add/remove items from a hash cache.
/// It is not designed to be a generic map class. It uses specific
/// key/value pairs and only implements necessary APIs used in this project.

template<typename ValueType>
class ConcurrentMap {
public:
    ConcurrentMap() = default;
    ~ConcurrentMap() = default;

    ConcurrentMap(const ConcurrentMap& other)
    {
        std::lock_guard<std::mutex> lk(other.m_cacheMutex);
        m_cache = other.m_cache;
    }

    ConcurrentMap(ConcurrentMap&& other)
    {
        std::lock_guard<std::mutex> lk(other.m_cacheMutex);
        m_cache = std::move(other.m_cache);
    }

    ConcurrentMap & operator=(const ConcurrentMap& other)
    {
        if (this != &other) {
            std::unique_lock<std::mutex> lhs_lk(m_cacheMutex, std::defer_lock);
            std::unique_lock<std::mutex> rhs_lk(other.m_cacheMutex, std::defer_lock);
            std::lock(lhs_lk, rhs_lk);
            m_cache = other.m_cache;
        }
        return *this;
    }

    ConcurrentMap & operator=(ConcurrentMap&& other)
    {
        if (this != &other) {
            std::unique_lock<std::mutex> lhs_lk(m_cacheMutex, std::defer_lock);
            std::unique_lock<std::mutex> rhs_lk(other.m_cacheMutex, std::defer_lock);
            std::lock(lhs_lk, rhs_lk);
            m_cache = std::move(other.m_cache);
        }
        return *this;
    }

    /// Add new key, value pair
    /// If key exists, old entry will be replaced.
    void Add(const std::string & key, ValueType value)
    {
        if (key.empty()) {
            throw std::invalid_argument("Invalid empty string for map key.");
        }

        std::lock_guard<std::mutex> lk(m_cacheMutex);
        m_cache[key] = std::move(value);
    }

    /// Erase an item with given key
    /// Return 1 if erased, 0 if nothing is erased.
    size_t Erase(const std::string & key)
    {
        if (key.empty()) {
            return 0;
        }

        std::lock_guard<std::mutex> lk(m_cacheMutex);
        auto nErased = m_cache.erase(key);
        return nErased;
    }

    /// Erase a list of items given their keys
    /// Return number of items erased.
    size_t Erase(const std::vector<std::string>& keylist)
    {
        if (keylist.empty()) {
            return 0;
        }

        size_t nTotal = 0;
        std::lock_guard<std::mutex> lk(m_cacheMutex);
        for(const auto & key : keylist) {
            auto nErased = m_cache.erase(key);
            nTotal += nErased;
        }

        return nTotal;
    }

    /// Return all the keys such that fn(value) == true.
    std::vector<std::string> FilterEach(const std::function<bool(ValueType)>& fn)
    {
        std::vector<std::string> keylist;
        std::lock_guard<std::mutex> lk(m_cacheMutex);

        for(const auto & item : m_cache) {
            if(fn(item.second)) {
                keylist.push_back(item.first);
            }
        }
        return keylist;
    }

    /// Apply fn on each value of the cache.
    void ForEach(const std::function<void(ValueType)>& fn)
    {
        std::lock_guard<std::mutex> lk(m_cacheMutex);
        ForEachUnsafe(fn);
    }

    /// Apply fn on each value of the cache without locking the mutex.
    /// This is not designed to be thread-safe.
    void ForEachUnsafe(const std::function<void(ValueType)>& fn)
    {
        std::for_each(m_cache.begin(), m_cache.end(),
            [fn](typename decltype(m_cache)::value_type & item) { fn(item.second); });
    }

    /// Get an entry with the key.
    /// If the key doesn't exist, throw std::out_of_range exception.
    ValueType Get(const std::string & key)
    {
        if (key.empty()) {
            throw std::invalid_argument("Invalid empty string for map key.");
        }
        std::lock_guard<std::mutex> lk(m_cacheMutex);
        auto item = m_cache.find(key);
        if (item == m_cache.end()) {
            throw std::out_of_range("ConcurrentMap::Get(): key not found " + key);
        }
        return item->second;
    }

    size_t Size() const
    {
        std::lock_guard<std::mutex> lk(m_cacheMutex);
        return m_cache.size();
    }

private:
    std::unordered_map<std::string, ValueType> m_cache;
    mutable std::mutex m_cacheMutex;
};

} // namespace
#endif // __ENDPOINTLOG_CONCURRENTMAP_H__
