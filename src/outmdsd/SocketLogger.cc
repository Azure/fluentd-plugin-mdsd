#include <cassert>

#include "ConcurrentMap.h"
#include "SocketLogger.h"
#include "Trace.h"
#include "TraceMacros.h"
#include "SocketClient.h"
#include "DataReader.h"
#include "DataResender.h"
#include "DjsonLogItem.h"
#include "Exceptions.h"

using namespace EndpointLog;

SocketLogger::SocketLogger(
    const std::string& socketFile,
    unsigned int ackTimeoutMS,
    unsigned int resendIntervalMS,
    unsigned int connRetryTimeoutMS
    ):
    m_socketClient(std::make_shared<SocketClient>(socketFile, connRetryTimeoutMS)),
    m_dataCache(ackTimeoutMS? std::make_shared<ConcurrentMap<LogItemPtr>>() : nullptr),
    m_sockReader(new DataReader(m_socketClient, m_dataCache)),
    m_dataResender(ackTimeoutMS?
        new DataResender(m_socketClient, m_dataCache, ackTimeoutMS, resendIntervalMS) : nullptr)
{
}

SocketLogger::~SocketLogger()
{
    try {
        ADD_INFO_TRACE;

        m_socketClient->Stop();

        if (m_dataResender) {
            m_dataResender->Stop();
        }
        m_sockReader->Stop();

        for(auto & task : m_workerTasks) {
            if (task.valid()) {
                task.get();
            }
        }
    }
    catch(const std::exception& ex)
    {
        Log(TraceLevel::Error, "unexpected exception: " << ex.what());
    }
    catch(...)
    {} // no exception thrown from destructor
}

void
SocketLogger::StartWorkers()
{
    m_workerTasks.push_back(std::async(std::launch::async, [this] { m_sockReader->Run(); }));
    if (m_dataResender) {
        m_workerTasks.push_back(std::async(std::launch::async, [this] {  m_dataResender->Run(); }));
    }
}

void
SocketLogger::SendData(
    LogItemPtr item
    )
{
    ADD_DEBUG_TRACE;

    if (!item) {
        throw std::invalid_argument("SendData(): unexpected NULL in input parameter.");
    }

    std::call_once(m_initOnceFlag, &SocketLogger::StartWorkers, this);

    if (!m_dataCache) {
        // If no caching, send it out immediately
        m_socketClient->Send(item->GetData());
        m_totalSend++;
    }
    else {
        // Move item to cache first before sending it out.
        // This makes sure that the cache has the tag in the thread
        // where response is received and handled.
        item->Touch();
        auto tag = item->GetTag();
        m_dataCache->Add(tag, std::move(item));

        auto dataItem = m_dataCache->Get(tag);
        assert(dataItem);
        try {
            m_socketClient->Send(m_dataCache->Get(tag)->GetData());
            m_totalSend++;
        }
        catch(...) {
            // if Send() fails, the caller of SocketLogger is expected to
            // retry, so remove it from cache.
            auto nErased = m_dataCache->Erase(tag);
            Log(TraceLevel::Trace, "Send() failed on msgid='" << tag << "'; Try to erase. nErased=" << nErased);
            throw;
        }
    }
}

bool
SocketLogger::SendDjson(
    const std::string & sourceName,
    const std::string & schemaAndData
    )
{
    ADD_DEBUG_TRACE;

    if (sourceName.empty()) {
        Log(TraceLevel::Error, "SendDjson: unexpected empty source name.");
        return false;
    }
    if (schemaAndData.empty()) {
        Log(TraceLevel::Error, "SendDjson: unexpected empty schemaAndData string.");
        return false;
    }
    try {
        LogItemPtr item(new DjsonLogItem(sourceName, schemaAndData));
        SendData(std::move(item));
        return true;
    }
    catch(const SocketException & ex) {
        Log(TraceLevel::Error, "SendDjson SocketException: " << ex.what());
    }
    catch(const std::exception& ex) {
        Log(TraceLevel::Error, "SendDjson exception: " << ex.what());
    }
    catch(...) {
        Log(TraceLevel::Error, "SendDjson hit unknown exception");
    }
    return false;
}

size_t
SocketLogger::GetNumTagsRead() const
{
    return m_sockReader->GetNumTagsRead();
}

size_t
SocketLogger::GetTotalResend() const
{
    return (m_dataResender? m_dataResender->GetTotalSendTimes() : 0);
}

size_t
SocketLogger::GetNumItemsInCache() const
{
    return (m_dataCache? m_dataCache->Size() : 0);
}

