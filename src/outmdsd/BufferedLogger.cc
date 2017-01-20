#include "ConcurrentMap.h"
#include "ConcurrentQueue.h"
#include "BufferedLogger.h"
#include "Trace.h"
#include "TraceMacros.h"
#include "SocketClient.h"
#include "DataReader.h"
#include "DataResender.h"
#include "DataSender.h"

using namespace EndpointLog;

BufferedLogger::BufferedLogger(
    const std::string& socketFile,
    unsigned int ackTimeoutMS,
    unsigned int resendIntervalMS,
    unsigned int connRetryTimeoutMS,
    size_t bufferLimit
    ):
    m_sockClient(std::make_shared<SocketClient>(socketFile, connRetryTimeoutMS)),
    m_dataCache(ackTimeoutMS? std::make_shared<ConcurrentMap<LogItemPtr>>() : nullptr),
    m_incomingQueue(std::make_shared<ConcurrentQueue<LogItemPtr>>(bufferLimit)),
    m_sockReader(new DataReader(m_sockClient, m_dataCache)),
    m_dataResender(ackTimeoutMS? new DataResender(m_sockClient, m_dataCache,
                   ackTimeoutMS, resendIntervalMS): nullptr),
    m_dataSender(new DataSender(m_sockClient, m_dataCache, m_incomingQueue))
{
}

BufferedLogger::~BufferedLogger()
{
    try {
        ADD_INFO_TRACE;

        m_sockClient->Stop();
        m_incomingQueue->stop_once_empty();

        m_dataSender->Stop();
        if (m_dataResender) {
            m_dataResender->Stop();
        }
        m_sockReader->Stop();

        if (m_senderTask.valid()) {
            m_senderTask.get();
        }
        if (m_resenderTask.valid()) {
            m_resenderTask.get();
        }
        if (m_readerTask.valid()) {
            m_readerTask.get();
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
BufferedLogger::StartWorkers()
{
    m_senderTask = std::async(std::launch::async, [this] { m_dataSender->Run(); });
    m_readerTask = std::async(std::launch::async, [this] { m_sockReader->Run(); });
    if (m_dataResender) {
        m_resenderTask = std::async(std::launch::async, [this] {  m_dataResender->Run(); });
    }
}

void
BufferedLogger::AddData(
    LogItemPtr item
    )
{
    if (!item) {
        throw std::invalid_argument("AddData(): unexpected NULL in input parameter.");
    }
    std::call_once(m_initOnceFlag, &BufferedLogger::StartWorkers, this);
    m_incomingQueue->push(std::move(item));
}

bool
BufferedLogger::WaitUntilAllSend(
    uint32_t timeoutMS
    )
{
    ADD_DEBUG_TRACE;
    m_incomingQueue->stop_once_empty();
    auto status = m_senderTask.wait_for(std::chrono::milliseconds(timeoutMS));
    return (std::future_status::ready == status);
}


size_t
BufferedLogger::GetNumTagsRead() const
{
    return m_sockReader->GetNumTagsRead();
}

size_t
BufferedLogger::GetTotalSend() const
{
    return  m_dataSender->GetNumSend() + GetTotalResend();
}

size_t
BufferedLogger::GetTotalSendSuccess() const
{
    return m_dataSender->GetNumSuccess();
}

size_t
BufferedLogger::GetTotalResend() const
{
    return (m_dataResender? m_dataResender->GetTotalSendTimes() : 0);
}

size_t
BufferedLogger::GetNumItemsInCache() const
{
    return (m_dataCache? m_dataCache->Size() : 0);
}
