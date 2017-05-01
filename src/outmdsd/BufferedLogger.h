#pragma once

#ifndef __ENDPOINTLOGGER_H__
#define __ENDPOINTLOGGER_H__

#include <future>
#include <memory>
#include "LogItemPtr.h"

namespace EndpointLog {

template<typename T> class ConcurrentQueue;
template<typename T> class ConcurrentMap;
class SocketClient;
class DataReader;
class DataResender;
class DataSender;

// This class implements a data logger to a socket server with retry when socket
// failure occurs. It uses multiple threads internally:
// - The main thread will add the data to a shared, concurrent queue then move to
//   next data item.
// - A sender thread will pop and send data from the queue to the socket server.
// - A reader thread will read and process ack data from the socket server.
// - An optional resender thread will resend data that failed to be sent before.
//
class BufferedLogger
{
public:
    /// <summary>
    /// Construct a logger that'll send data to a Unix domain socket.
    ///
    /// <param name='socketFile'> full path to socket file </param>
    /// <param name='ackTimeoutMS'> max milliseconds to wait for ack from socket server.
    /// After timeout, record will be dropped from cache. If this parameter's value 
    /// is 0, no data will be cached. </param>
    /// <param name='resendIntervalMS'>message resend interval in milliseconds.</param>
    /// <param name='connRetryTimeoutMS'>number of milliseconds to timeout socket
    /// connect() retry </param>
    /// <param name='bufferLimit'>max LogItem to buffer. 0 means no limit.</param>
    BufferedLogger(
        const std::string& socketFile,
        unsigned int ackTimeoutMS,
        unsigned int resendIntervalMS,
        unsigned int connRetryTimeoutMS,
        size_t bufferLimit
        );

    ~BufferedLogger();

    BufferedLogger(const BufferedLogger&) = delete;
    BufferedLogger& operator=(const BufferedLogger &) = delete;

    BufferedLogger(BufferedLogger&& h) = delete;
    BufferedLogger& operator=(BufferedLogger&& h) = delete;

    /// <summary>
    /// Add new data item to logger.
    /// Throw exception for any error.
    /// </summary>
    /// <param name='item'>A new logger item.</param>
    void AddData(LogItemPtr item);

    /// Wait until all the items are sent by the sender thread or timed out.
    /// Return true if all the items are sent out, false if timed out.
    bool WaitUntilAllSend(uint32_t timeoutMS);

    /// Return total number of ack tags processed by reader thread.
    size_t GetNumTagsRead() const;

    /// Return total number of Send() called, including sender thread and resender thread.
    size_t GetTotalSend() const;

    /// Return total number of successful Send() by sender thread.
    size_t GetTotalSendSuccess() const;

    /// Return total number of Send() called by data resender
    size_t GetTotalResend() const;

    /// Return number of items in the backup cache, either not resent yet,
    /// or not acknowledged yet
    size_t GetNumItemsInCache() const;

private:
    void StartWorkers();

private:
    std::shared_ptr<SocketClient> m_sockClient;
    std::shared_ptr<ConcurrentMap<LogItemPtr>> m_dataCache; // to store <TAG, Item>
    std::shared_ptr<ConcurrentQueue<LogItemPtr>> m_incomingQueue; // to store incoming data item.

    std::future<void> m_senderTask;
    std::future<void> m_readerTask;
    std::future<void> m_resenderTask;

    std::unique_ptr<DataReader> m_sockReader;     // to read ack from socket server.
    std::unique_ptr<DataResender> m_dataResender; // to resend failed data to socket server.
    std::unique_ptr<DataSender> m_dataSender;     // to send data to socket server.

    std::once_flag m_initOnceFlag; // a flag to make sure something is called exactly once.
};

} // namespace

#endif // __ENDPOINTLOGGER_H__
