#pragma once
#ifndef __ENDPOINTLOG_IDMGR_H__
#define __ENDPOINTLOG_IDMGR_H__

#include <string>
#include <unordered_map>
#include <mutex>

namespace EndpointLog {

// This class manages a cache for concurrent access. Cache key is
// is std::string, cache value is 'value_type_t', a <id,string>.
//
class IdMgr {
public:
    using value_type_t = std::pair<uint64_t, std::string>;

    /// Get an item given key. Return the item from 'result'.
    /// Return true if the key is found and returned by 'result',
    /// Return false if the key is not found.
    /// Throw exception if key is a empty string.
    bool GetItem(const std::string & key, value_type_t& result);

    /// Find or insert an item with given key.
    /// - If the key already exists, return the id part of value item;
    /// - If the key doesn't exist, get a unique id and add <id,data>
    ///   to the cache. Then return the new id.
    /// Throw exception if 'key' or 'data' is empty.
    uint64_t FindOrInsert(const std::string & key, const std::string & data);

    /// Insert an item with given key such that cache[key] = value if key doesn't exist.
    /// If the key already exists, do nothing.
    /// Throw exception if 'key' is empty, or value contains empty string.
    void Insert(const std::string & key, const value_type_t& value);

private:
    std::unordered_map<std::string, value_type_t> m_cache;
    std::mutex m_mutex;
};

} // namespace

#endif // __ENDPOINTLOG_IDMGR_H__
