#include "ConcurrentMap.h"
#include <cassert>
#include "Trace.h"

using namespace EndpointLog;

ConcurrentMap::ConcurrentMap(
    const ConcurrentMap& other
    )
{
    std::lock_guard<std::mutex> lk(other.m_cacheMutex);
    m_cache = other.m_cache;
}

ConcurrentMap::ConcurrentMap(
    ConcurrentMap&& other
    )
{
    std::lock_guard<std::mutex> lk(other.m_cacheMutex);
    m_cache = std::move(other.m_cache);
}

ConcurrentMap&
ConcurrentMap::operator=(const ConcurrentMap& other)
{
    if (this != &other) {
        std::unique_lock<std::mutex> lhs_lk(m_cacheMutex, std::defer_lock);
        std::unique_lock<std::mutex> rhs_lk(other.m_cacheMutex, std::defer_lock);
        std::lock(lhs_lk, rhs_lk);
        m_cache = other.m_cache;
    }
    return *this;
}

ConcurrentMap&
ConcurrentMap::operator=(ConcurrentMap&& other)
{
    if (this != &other) {
        std::unique_lock<std::mutex> lhs_lk(m_cacheMutex, std::defer_lock);
        std::unique_lock<std::mutex> rhs_lk(other.m_cacheMutex, std::defer_lock);
        std::lock(lhs_lk, rhs_lk);
        m_cache = std::move(other.m_cache);
    }
    return *this;
}

void
ConcurrentMap::Add(
    const std::string & key,
    LogItemPtr item)
{
    assert(!key.empty());
    assert(item);

    std::lock_guard<std::mutex> lk(m_cacheMutex);
    m_cache[key] = std::move(item);
}

LogItemPtr
ConcurrentMap::Get(
    const std::string & key
    )
{
    assert(!key.empty());
    std::lock_guard<std::mutex> lk(m_cacheMutex);
    auto item = m_cache.find(key);
    if (item == m_cache.end()) {
        return nullptr;
    }
    return item->second;
}

void
ConcurrentMap::Erase(
    const std::string & key
    )
{
    assert(!key.empty());

    std::lock_guard<std::mutex> lk(m_cacheMutex);
    auto nErased = m_cache.erase(key);
    Log(TraceLevel::Trace, "Erase(): key='" << key << "'; nErased=" << nErased);
}

void
ConcurrentMap::Erase(
    const std::vector<std::string> & keylist
    )
{
    if (keylist.empty()) {
        return;
    }

    std::lock_guard<std::mutex> lk(m_cacheMutex);
    for(const auto & key : keylist) {
        auto nErased = m_cache.erase(key);
        Log(TraceLevel::Trace, "Erase(): key='" << key << "'; nErased=" << nErased);
    }
}

size_t
ConcurrentMap::Size() const
{
    std::lock_guard<std::mutex> lk(m_cacheMutex);
    return m_cache.size();
}

std::vector<std::string>
ConcurrentMap::FilterEach(
    const std::function<bool(LogItemPtr)>& fn
    )
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

void
ConcurrentMap::ForEach(
    const std::function<void(LogItemPtr)>& fn
    )
{
    std::lock_guard<std::mutex> lk(m_cacheMutex);
    for(auto & item : m_cache) {
        fn(item.second);
    }
}

void
ConcurrentMap::ForEachUnsafe(
    const std::function<void(LogItemPtr)>& fn
    )
{
    for(auto & item : m_cache) {
        fn(item.second);
    }
}
