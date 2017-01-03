#include <cassert>

#include "ConcurrentMap.h"
#include "DataResender.h"
#include "SocketClient.h"
#include "Exceptions.h"
#include "Trace.h"
#include "LogItem.h"

using namespace EndpointLog;

DataResender::DataResender(
    const std::shared_ptr<SocketClient> & sockClient,
    const std::shared_ptr<ConcurrentMap<LogItemPtr>> & dataCache,
    int ackTimeoutMS,
    int resendIntervalMS
    ) :
    m_socketClient(sockClient),
    m_dataCache(dataCache),
    m_ackTimeoutMS(ackTimeoutMS),
    m_resendIntervalMS(resendIntervalMS)
{
    assert(m_socketClient);
    assert(m_dataCache);

    if (ackTimeoutMS <= 0) {
        throw std::invalid_argument("DataResender: ack timeout must be a positive integer.");
    }

    if (m_resendIntervalMS <= 0) {
        throw std::invalid_argument("DataResender: resend interval must be a positive integer.");
    }

}

DataResender::~DataResender()
{
    try {
        Stop();
    }
    catch(const std::exception & ex) {
        Log(TraceLevel::Error, "~DataResender() exception: " << ex.what());
    }
    catch(...) {
    } // no exception thrown from destructor
}

size_t
DataResender::Run()
{
    ADD_INFO_TRACE;

    size_t counter = 0;

    while(!m_stopMe) {
        WaitForNextResend();
        if (m_stopMe) {
            break;
        }

        ResendOnce();
        counter++;
    }

    Log(TraceLevel::Debug, "DataResender finished: total resend round: " << counter << ".");
    return counter;
}

void
DataResender::Stop()
{
    ADD_INFO_TRACE;

    std::lock_guard<std::mutex> lck(m_timerMutex);
    m_stopMe = true;
    m_timerCV.notify_one();
}

void
DataResender::WaitForNextResend()
{
    ADD_TRACE_TRACE;
    std::unique_lock<std::mutex> lck(m_timerMutex);

    // Wait for m_resendIntervalMS until it is told to abort by m_stopMe
    m_timerCV.wait_for(lck, std::chrono::milliseconds(m_resendIntervalMS), [this] {
        return m_stopMe.load();
    });
}

void
DataResender::ResendOnce()
{
    ADD_TRACE_TRACE;

    try {
        if (m_dataCache->Size() > 0) {
            ResendData();
        }
    }
    catch(const std::exception& ex) {
        Log(TraceLevel::Error, "DataResender send failed: " << ex.what());
    }
}

void
DataResender::ResendData()
{
    ADD_TRACE_TRACE;

    // Check whether any cached items need to be dropped
    std::function<bool(LogItemPtr)> CheckItemAge = [this](LogItemPtr itemPtr)
    {
        if (itemPtr && itemPtr->GetLastTouchMilliSeconds() > m_ackTimeoutMS) {
            return true;
        }
        return false;
    };

    // To minimize locking on m_dataCache, copy it first. Then run filter function
    // on the copied cache.
    auto cacheBak = *m_dataCache;
    auto keysToRemove = cacheBak.FilterEach(CheckItemAge);

    // The Erase() and FilterEach() are not protected together. It is OK that
    // some key is gone in the data cache when Erase() is called.
    m_dataCache->Erase(keysToRemove);

    for (const auto & key : keysToRemove) {
        Log(TraceLevel::Trace, "obsolete key erased: '" << key << "'.");
    }

    // To minimize locking on m_dataCache, copy it first. Then send data in the
    // copied cache.
    auto cacheCopy = *m_dataCache;

    try {
        std::function<void(LogItemPtr)> SendItem = [this](LogItemPtr itemPtr)
        {
            if (itemPtr) {
                m_socketClient->Send(itemPtr->GetData());
                m_totalSend++;
            }
        };

        cacheCopy.ForEachUnsafe(SendItem);
        Log(TraceLevel::Trace, "ResendData(): m_totalSend=" << m_totalSend);
    }
    catch(const SocketException & ex) {
        Log(TraceLevel::Info, "SocketException: " << ex.what());
    }
}
