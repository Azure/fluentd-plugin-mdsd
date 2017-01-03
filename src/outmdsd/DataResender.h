#pragma once
#ifndef __ENDPOINT_DATARESENDER_H__
#define __ENDPOINT_DATARESENDER_H__

#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>

#include "LogItemPtr.h"

namespace EndpointLog {

template<typename T> class ConcurrentMap;
class SocketClient;

/// This class will resend data in a shared cache to a socket server
/// in a multi-thread system, while other threads keep on inserting data to
/// the same shared cache.
///
/// It will run in a resend-sleep loop until it is told to stop.
///
/// It reads data and removes obsolete data from the shared cache. It doesn't
/// add data to the cache.
///
class DataResender {
public:
    /// Constructor.
    /// <param name="sockClient"> shared socket client object</param>
    /// <param name="dataCache"> shared cache. DataResender reads and removes data from it.
    /// Other threads will add items to it.</param>
    /// <param name="ackTimeoutMS">max milli-seconds to wait for socket server acknowledge
    /// before removing data from cache. </param>
    /// <param name="resendIntervalMS">milli-seconds to do resending</param>
    DataResender(
        const std::shared_ptr<SocketClient> & sockClient,
        const std::shared_ptr<ConcurrentMap<LogItemPtr>> & dataCache,
        int ackTimeoutMS,
        int resendIntervalMS
        );

    ~DataResender();

    // not copyable, not movable
    DataResender(const DataResender& other) = delete;
    DataResender& operator=(const DataResender& other) = delete;

    DataResender(DataResender&& other) = delete;
    DataResender& operator=(DataResender&& other) = delete;

    /// <summary>
    /// Run the resend-sleep loop. It will loop forever until it is told to stop.
    /// Return the number of timers triggered.
    /// </summary>
    size_t Run();

    /// <summary>
    /// Told the resending loop to stop. Typically called in a thread different
    /// from the one calls Run().
    /// </summary>
    void Stop();

    size_t GetTotalSendTimes() const { return m_totalSend; }

private:
    /// <summary>Wait for m_resendIntervalMS before next resending turn.</summary>
    void WaitForNextResend();

    /// <summary>Resend all valid data and handle exceptions.</summary>
    void ResendOnce();

    /// <summary>
    /// Resend all valid data in the cache to the socket server.
    /// Obsolete data based on ack timeout will be removed before being resent.
    /// </summary>
    void ResendData();

private:
    std::shared_ptr<SocketClient> m_socketClient;
    std::shared_ptr<ConcurrentMap<LogItemPtr>> m_dataCache;

    int m_ackTimeoutMS;       // if ack is not received in this time, item is removed from cache.
    int m_resendIntervalMS;   // cached items resending interval in milli-seconds.

    std::atomic<bool> m_stopMe { false }; // A flag used to stop resending loop.

    /// m_timerMutex and m_timerCV are used to create an interruptable blocking wait.
    std::mutex m_timerMutex;
    std::condition_variable m_timerCV;

    std::atomic<size_t> m_totalSend {0}; // total Send() is called on socket. for testability
};

} // namespace

#endif // __ENDPOINT_DATARESENDER_H__
