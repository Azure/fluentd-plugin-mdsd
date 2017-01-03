#pragma once

#ifndef __SOCKETLOGGER_H__
#define __SOCKETLOGGER_H__

#include <future>
#include <memory>
#include <vector>
#include <atomic>
#include "LogItemPtr.h"

namespace EndpointLog {

template<typename T> class ConcurrentMap;
class SocketClient;
class DataReader;
class DataResender;

class SocketLogger
{
public:
    /// <summary>
    /// Contruct a logger that'll send data to a Unix domain socket.
    ///
    /// <param name='socketFile'> full path to socket file </param>
    /// <param name='ackTimeoutMS'> max milli-seconds to wait for ack from socket server.
    /// After timeout, record will be dropped from cache. If this parameter's value 
    /// is 0, do no caching. </param>
    /// <param name='resendIntervalMS'>message resend interval in milli-seconds
    /// from this logger to the targetted endpoint.
    /// </param>
    SocketLogger(
        const std::string& socketFile,
        int ackTimeoutMS,
        int resendIntervalMS,
        int connRetryTimeoutMS = 60*1000
        );

    ~SocketLogger();

    // not copyable, not movable
    SocketLogger(const SocketLogger&) = delete;
    SocketLogger& operator=(const SocketLogger &) = delete;

    SocketLogger(SocketLogger&& h) = delete;
    SocketLogger& operator=(SocketLogger&& h) = delete;

    /// Send a dynamic json data to mdsd socket.
    /// sourceName: source name of the event.
    /// schemaAndData: a string containing schema info and actual data values.
    /// Return true if success, false if any error.
    bool SendDjson(const std::string & sourceName, const std::string & schemaAndData);

    /// Return total number of ack tags processed by reader thread.
    size_t GetNumTagsRead() const;

    /// Return total Send() called, including main thread and resender thread.
    size_t GetTotalSend() const {
        return m_totalSend + GetTotalResend();
    }

    /// Return total number of Send() called by data resender
    size_t GetTotalResend() const;

    /// Return number of items in the backup cache, either not resent yet,
    /// or not acknowledged yet
    size_t GetNumItemsInCache() const;

private:
    void StartWorkers();

    /// <summary>
    /// Send new data item to socket.
    /// Throw exception for any error.
    /// </summary>
    /// <param name='item'>A new logger item.</param>
    void SendData(LogItemPtr item);

private:
    std::shared_ptr<SocketClient> m_socketClient;
    std::shared_ptr<ConcurrentMap<LogItemPtr>> m_dataCache; // to store <TAG, Item>

    std::vector<std::future<void>> m_workerTasks; // store all the worker tasks (reader, resender)

    std::unique_ptr<DataReader> m_sockReader;
    std::unique_ptr<DataResender> m_dataResender;

    std::once_flag m_initOnceFlag;

    std::atomic<size_t> m_totalSend{0}; // a counter. it includes main thread Send() to socket only
};

} // namespace

#endif // __SOCKETLOGGER_H__
