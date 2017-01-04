#pragma once
#ifndef _CONCURRENT_QUEUE_H__
#define _CONCURRENT_QUEUE_H__

#include <mutex>
#include <condition_variable>
#include <queue>
#include <memory>
#include <atomic>

namespace EndpointLog {

/// This class implements a thread-safe concurrent queue with an optional max size limit.
/// When a max size is set, after max size is reached, before pushing new element, oldest element
/// will be popped and dropped.
///
/// Most of the code are from "C++ Concurrency In Action" book
/// source code listing 4.5.
/// https://manning-content.s3.amazonaws.com/download/0/78f6c43-a41b-4eb0-82f2-44c24eba51ad/CCiA_SourceCode.zip
template<typename T>
class ConcurrentQueue
{
private:
    mutable std::mutex mut;  // mutex to lock the queue
    std::queue<T> data_queue; // underling queue
    std::condition_variable data_cond; // CV for queue synchronization
    std::atomic<bool> stopOnceEmpty{false}; // if true, notify CV to stop any further waiting once queue is empty
    size_t m_maxSize; // max number of items to hold in the queue. 0 means it will limited by std::queue's max size.

public:
    ConcurrentQueue(size_t maxSize = 0) : m_maxSize(maxSize)
    {
    }

    ConcurrentQueue(const ConcurrentQueue& other) 
    {
        std::lock_guard<std::mutex> lk(other.mut);
        data_queue = other.data_queue;
        m_maxSize = other.m_maxSize;
    }

    ConcurrentQueue(ConcurrentQueue&& other)
    {
        std::lock_guard<std::mutex> lk(other.mut);
        data_queue = std::move(other.data_queue);
        data_cond = std::move(other.data_cond);
        m_maxSize = other.m_maxSize;
    }

    ConcurrentQueue& operator=(const ConcurrentQueue&) = delete;
    ConcurrentQueue& operator=(ConcurrentQueue&&) = delete;

    ~ConcurrentQueue()
    {
        stop_once_empty();
    }

    void push(const T & new_value)
    {
        std::lock_guard<std::mutex> lk(mut);
        if (is_full()) {
            data_queue.pop();
        }
        data_queue.push(new_value);
        data_cond.notify_one();
    }

    void push(T && new_value)
    {
        std::lock_guard<std::mutex> lk(mut);
        if (is_full()) {
            data_queue.pop();
        }
        data_queue.emplace(std::move(new_value));
        data_cond.notify_one();
    }

    /// Wait until any item is available in the queue, then pop it
    void wait_and_pop(T& value)
    {
        std::unique_lock<std::mutex> lk(mut);
        data_cond.wait(lk,[this]{ return (!data_queue.empty() || stopOnceEmpty);});
        if (data_queue.empty() && stopOnceEmpty) {
            return;
        }
        value = data_queue.front();
        data_queue.pop();
    }

    /// Wait until any item is available in the queue, then pop it
    std::shared_ptr<T> wait_and_pop()
    {
        std::unique_lock<std::mutex> lk(mut);
        data_cond.wait(lk,[this]{return (!data_queue.empty() || stopOnceEmpty);});
        if (data_queue.empty() && stopOnceEmpty) {
            return nullptr;
        }
        std::shared_ptr<T> res(std::make_shared<T>(data_queue.front()));
        data_queue.pop();
        return res;
    }

    /// If queue is empty, pop nothing, return false.
    /// If queue is not empty, pop and return the popped item.
    bool try_pop(T& value)
    {
        std::lock_guard<std::mutex> lk(mut);
        if(data_queue.empty()) {
            return false;
        }
        value = data_queue.front();
        data_queue.pop();
    }

    /// If queue is empty, pop nothing, return false.
    /// If queue is not empty, pop and return the popped item.
    std::shared_ptr<T> try_pop()
    {
        std::lock_guard<std::mutex> lk(mut);
        if(data_queue.empty()) {
            return std::shared_ptr<T>();
        }
        std::shared_ptr<T> res(std::make_shared<T>(data_queue.front()));
        data_queue.pop();
        return res;
    }

    bool empty() const
    {
        std::lock_guard<std::mutex> lk(mut);
        return data_queue.empty();
    }

    size_t size() const
    {
        std::lock_guard<std::mutex> lk(mut);
        return data_queue.size();
    }

    /// Notify queue to stop any further waiting once it is empty.
    /// If queue is not empty, continue.
    void stop_once_empty()
    {
        std::lock_guard<std::mutex> lk(mut);
        stopOnceEmpty = true;
        data_cond.notify_all();
    }
private:
    bool is_full() const {
        return (0 < m_maxSize && data_queue.size() == m_maxSize);
    }
};

} // namespace

#endif // _CONCURRENTQUEUE_H__
