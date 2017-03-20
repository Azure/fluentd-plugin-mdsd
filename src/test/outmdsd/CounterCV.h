#pragma once
#ifndef __COUNTERCV_H__
#define __COUNTERCV_H__

#include <condition_variable>
#include <chrono>
#include <mutex>
#include <memory>


// This class implements a synchronization class that can wait for
// a counter reachs 0. It is similar to semaphore but it is more
// specific.
class CounterCV
{
public:
    // constructor
    CounterCV(uint32_t count) : m_count(count) {}

    ~CounterCV() = default;

    // decrease counter by 1 and notify one
    void notify_one()
    {
        std::lock_guard<std::mutex> lck(m_mutex);
        if (m_count > 0) {
            m_count--;
            m_cv.notify_one();
        }
    }

    // decrease counter by 1 and notify all
    void notify_all()
    {
        std::lock_guard<std::mutex> lck(m_mutex);
        if (m_count > 0) {
            m_count--;
            m_cv.notify_all();
        }
    }

    // wait for counter to be 0 or timeout.
    // Return true if counter is 0 at exit, return false otherwise.
    bool wait_for(uint32_t timeoutMS)
    {
        std::unique_lock<std::mutex> lck(m_mutex);
        return m_cv.wait_for(lck, std::chrono::milliseconds(timeoutMS), [this] () 
        {
            return (0 == m_count);
        });
    }

    void wait()
    {
        std::unique_lock<std::mutex> lck(m_mutex);
        m_cv.wait(lck, [this] () { return (0 == m_count); });
    }

    uint32_t GetId() const
    {
        return m_count;
    }

private:
    std::mutex m_mutex;
    std::condition_variable m_cv;
    uint32_t m_count;
};

// This wrapper class will decrease counter and notify all waiting CV
// in its destructor. It can be used as RAII to make sure CV is notified.
class CounterCVWrap
{
public:
    CounterCVWrap(const std::shared_ptr<CounterCV>& cv) : m_cv(cv) {}

    ~CounterCVWrap()
    {
        m_cv->notify_all();
    }

    uint32_t GetId() const
    {
        return m_cv->GetId();
    }

private:
    std::shared_ptr<CounterCV> m_cv;
};


#endif // __COUNTERCV_H__