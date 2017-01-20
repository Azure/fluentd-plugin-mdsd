#include <cassert>
#include "ConcurrentMap.h"
#include "ConcurrentQueue.h"
#include "DataSender.h"
#include "SocketClient.h"
#include "Exceptions.h"
#include "Trace.h"
#include "TraceMacros.h"
#include "LogItem.h"

using namespace EndpointLog;

class InterruptException {};

DataSender::DataSender(
    const std::shared_ptr<SocketClient> & sockClient,
    const std::shared_ptr<ConcurrentMap<LogItemPtr>> & dataCache,
    const std::shared_ptr<ConcurrentQueue<LogItemPtr>> & incomingQueue
    ) :
    m_socketClient(sockClient),
    m_dataCache(dataCache),
    m_incomingQueue(incomingQueue)
{
    assert(m_socketClient);
    assert(m_incomingQueue);
}

DataSender::~DataSender()
{
    try {
        Stop();
    }
    catch(const std::exception & ex) {
        Log(TraceLevel::Error, "~DataSender() exception: " << ex.what());
    }
    catch(...) {
    } // no exception thrown from destructor
}

void
DataSender::Stop()
{
    ADD_INFO_TRACE;
    m_stopSender = true;
}

void
DataSender::InterruptPoint() const
{
    if (m_stopSender) {
        throw InterruptException();
    }
}

void
DataSender::Run()
{
    try {
        ADD_INFO_TRACE;

        while(!m_stopSender) {
            LogItemPtr item;
            m_incomingQueue->wait_and_pop(item);

            // When ConcurrentQueue is empty and stopped, wait_and_pop() do nothing,
            // so item is still pointing to nullptr
            if (!item) {
                assert(0 == m_incomingQueue->size());
                Log(TraceLevel::Info, "Abort Run() because data queue is aborted.");
                break;
            }

            InterruptPoint();

            if (!m_dataCache) {
                // If no caching, send it out immediately
                Send(item->GetData());
            }
            else {
                // Move item to cache first before sending it out.
                // This makes sure that the cache has the tag in the thread
                // where response is received and handled.
                item->Touch();
                auto tag = item->GetTag();
                m_dataCache->Add(tag, std::move(item));

                auto dataItem = m_dataCache->Get(tag);
                if (dataItem) {
                    InterruptPoint();
                    Send(m_dataCache->Get(tag)->GetData());
                }
            }

            InterruptPoint();
        }
    }
    catch(const InterruptException&) {
        Log(TraceLevel::Debug, "DataSender is interrupted. Abort now.");
    }
    catch(const std::exception & ex) {
        Log(TraceLevel::Error, "DataSender hits unexpected exception: " << ex.what());
    } 
}

// Send data and catch SocketException.
// Because DataResender can keep on resending the failed data until timed out,
// it shouldn't abort further DataSender::Run(), and also log this as an information.
void
DataSender::Send(const char* data)
{
    try {
        m_numSend++;
        m_socketClient->Send(data);
        m_numSuccess++;
    }
    catch(const SocketException & ex) {
        Log(TraceLevel::Info, "DataSender Send() SocketException: " << ex.what());
    }
}
