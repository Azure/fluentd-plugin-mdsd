#pragma once

#ifndef __ENDPOINT_LOGITEM_H__
#define __ENDPOINT_LOGITEM_H__

#include <string>
#include <chrono>
#include <atomic>

namespace EndpointLog {

/// This class contains all the data to send to mdsd socket.
/// It has two parts: a tag and raw string data.
///
/// Because a tag must be unique during a certain amount of time (1-hour)
/// for current mdsd design, use a 64-bit counter to make it unique.
///
/// This is an abstract class. Its subclass will implement the details for
/// the real data operations.
class LogItem
{
public:
    LogItem() :
    m_tag(std::to_string(++LogItem::s_counter)),
    m_touchTime(std::chrono::system_clock::now())
    {
    }

    virtual ~LogItem() {}

    LogItem(const LogItem & other) = default;
    LogItem(LogItem&& other) = default;

    LogItem& operator=(const LogItem & other) = default;
    LogItem& operator=(LogItem&& other) = default;

    virtual std::string GetTag() const { return m_tag; }

    virtual const char* GetData() const = 0;

    void Touch() {
        m_touchTime = std::chrono::system_clock::now();
    }

    /// Return number of milli-seconds passed since the item is last touched.
    /// If never touched before, it will count from creation time.
    int GetLastTouchMilliSeconds() const 
    {
        auto now = std::chrono::system_clock::now();
        return (now - m_touchTime) / std::chrono::milliseconds(1);
    }

private:
    std::string m_tag;   // Tag to the log item.
    std::chrono::system_clock::time_point m_touchTime; // last touch time

    static std::atomic<uint64_t> s_counter; // counter of number of logItem created.
};

} // namespace EndpointLog

#endif // __ENDPOINT_LOGITEM_H__
