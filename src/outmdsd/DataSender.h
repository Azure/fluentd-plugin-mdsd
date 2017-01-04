#pragma once
#ifndef __ENDPOINT_DATASENDER_H__
#define __ENDPOINT_DATASENDER_H__

#include <memory>
#include <atomic>

#include "LogItemPtr.h"

namespace EndpointLog {

template<typename T> class ConcurrentQueue;
template<typename T> class ConcurrentMap;
class SocketClient;

/// This class will keep on sending incoming data in a shared queue to a
/// socket server in a multi-thread system. Other threads will keep on
/// pushing new data to the same shared queue (see BufferedLogger class).
///
/// DataSender will run in an infinite loop to pop and send each item
/// from the shared queue to socket server. If no item to pop, it will
/// wait until there is item in the queue. 
///
/// To avoid message loss, each item can be optionally saved to a cache for
/// future resend (see DataResender class).
///
class DataSender {
public:
    /// <summary>
    /// Constructor.
    /// <param name="sockClient"> socket client object</param>
    /// <param name="dataCache"> cache for data backup if not NULL. If NULL, no backup</param>
    /// <param name="incomingQueue"> data to be sent. DataSender will pop each item in the queue
    /// and send it to socket server. If no data to pop, it will wait until there is data to pop.
    /// </param>
    DataSender(
        const std::shared_ptr<SocketClient> & sockClient,
        const std::shared_ptr<ConcurrentMap<LogItemPtr>> & dataCache,
        const std::shared_ptr<ConcurrentQueue<LogItemPtr>> & incomingQueue
        );

    ~DataSender();

    // DataSender is not copyable but movable.
    DataSender(const DataSender& other) = delete;
    DataSender& operator=(const DataSender& other) = delete;

    DataSender(DataSender&& other) = default;
    DataSender& operator=(DataSender&& other) = default;

    /// <summary>
    /// Run sending process. It will run in an infinite loop until it is told to stop.
    /// It will pop and process each item in the incoming queue. If queue is empty,
    /// it will wait until queue has item.
    /// </summary>
    void Run();

    /// <summary>
    /// Notify sender to stop. This is typically called in a thread different from
    /// the Run() thread.
    /// </summary>
    void Stop();

    /// Return total number of send. including fails and successes.
    size_t GetNumSend() const { return m_numSend; }

    /// Return total number of successful send.
    size_t GetNumSuccess() const { return m_numSuccess; }

private:
    /// Define interruption point for Run() loop.
    void InterruptPoint() const;

    /// Send a data string. It will send all chars until it hits terminal NUL.
    /// NUL is not sent.
    void Send(const char* data);

private:
    std::shared_ptr<SocketClient> m_socketClient;
    std::shared_ptr<ConcurrentMap<LogItemPtr>> m_dataCache; // for data backup
    std::shared_ptr<ConcurrentQueue<LogItemPtr>> m_incomingQueue;  // incoming data queue

    std::atomic<bool> m_stopSender{false}; // a flag to notify sender loop to stop.

    size_t m_numSend = 0; // number of items trying to be sent. This includes fails and successes.
    size_t m_numSuccess = 0; // number of success send.
};

} // namespace

#endif // __ENDPOINT_DATASENDER_H__
