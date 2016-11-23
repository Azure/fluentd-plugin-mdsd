#pragma once
#ifndef _CONCURRENT_QUEUE_H__
#define _CONCURRENT_QUEUE_H__

#include <mutex>
#include <condition_variable>
#include <queue>
#include <memory>
#include <atomic>

/// This class implements a thread-safe concurrent queue.
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
    std::atomic<bool> stopwait_flag{false}; // if true, notify CV to stop any further waiting

public:
    ConcurrentQueue()
    {
    }

    ConcurrentQueue(const ConcurrentQueue& other) 
    {
        std::lock_guard<std::mutex> lk(other.mut);
        data_queue = other.data_queue;
    }

    ConcurrentQueue(ConcurrentQueue&& other)
    {
        std::lock_guard<std::mutex> lk(other.mut);
        data_queue = std::move(other.data_queue);
        data_cond = std::move(other.data_cond);
    }

    ConcurrentQueue& operator=(const ConcurrentQueue&) = delete;
    ConcurrentQueue& operator=(ConcurrentQueue&&) = delete;
    
    ~ConcurrentQueue()
    {
        stop_wait();
    }
    
    void push(const T & new_value)
    {
        std::lock_guard<std::mutex> lk(mut);
        data_queue.push(new_value);
        data_cond.notify_one();
    }

    void push(T && new_value)
    {
        std::lock_guard<std::mutex> lk(mut);
        data_queue.push(std::move(new_value));
        data_cond.notify_one();
    }

    /// Wait until any item is available in the queue, then pop it
    void wait_and_pop(T& value)
    {
        std::unique_lock<std::mutex> lk(mut);
        data_cond.wait(lk,[this]{ return (stopwait_flag || !data_queue.empty());});
        if (stopwait_flag) {
            return;
        }
        value=data_queue.front();
        data_queue.pop();
    }

    /// Wait until any item is available in the queue, then pop it
    std::shared_ptr<T> wait_and_pop()
    {
        std::unique_lock<std::mutex> lk(mut);
        data_cond.wait(lk,[this]{return (stopwait_flag || !data_queue.empty());});
        if (stopwait_flag) {
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
        value=data_queue.front();
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

    /// Notify queue to stop any further waiting
    void stop_wait()
    {
        std::lock_guard<std::mutex> lk(mut);
        stopwait_flag = true;
        data_cond.notify_all();
    }
};

#endif // _CONCURRENTQUEUE_H__
