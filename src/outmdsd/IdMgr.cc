#include <stdexcept>

#include "IdMgr.h"

namespace EndpointLog {

bool
IdMgr::GetItem(
    const std::string & key,
    value_type_t& result
    )
{
    if (key.empty()) {
        throw std::invalid_argument("GetItem(): invalid empty string for 'key' parameter.");
    }

    std::lock_guard<std::mutex> lck(m_mutex);

    auto iter = m_cache.find(key);
    if (iter == m_cache.end()) {
        return false;
    }
    else {
        result = iter->second;
        return true;
    }
}

uint64_t
IdMgr::FindOrInsert(
    const std::string & key,
    const std::string & data
    )
{
    if (key.empty()) {
        throw std::invalid_argument("FindOrInsert(): invalid empty string for 'key' parameter.");
    }
    if (data.empty()) {
        throw std::invalid_argument("FindOrInsert(): invalid empty string for 'data' parameter.");
    }

    std::lock_guard<std::mutex> lck(m_mutex);
    auto iter = m_cache.find(key);
    if (iter == m_cache.end()) {
        auto id = static_cast<uint64_t>(m_cache.size()+1);
        m_cache[key] = std::make_pair(id, data);
        return id;
    }
    else {
        if (data != iter->second.second) {
            throw std::runtime_error("FindOrInsert(): same key has diff values: expected=" +
                data + "; actual=" + iter->second.second);
        }
        return iter->second.first;
    }
}

void
IdMgr::Insert(
    const std::string & key,
    const value_type_t& value
    )
{
    if (key.empty()) {
        throw std::invalid_argument("Insert(): invalid empty string for 'key' parameter.");
    }
    if (value.second.empty()) {
        throw std::invalid_argument("Insert(): invalid empty string for 'value' parameter.");
    }

    std::lock_guard<std::mutex> lck(m_mutex);
    auto iter = m_cache.find(key);
    if (iter == m_cache.end()) {
        m_cache[key] = value;
    }
}

} // namespace
