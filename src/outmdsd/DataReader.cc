extern "C" {
#include <unistd.h>
}
#include <cassert>

#include "ConcurrentMap.h"
#include "DataReader.h"
#include "Exceptions.h"
#include "Trace.h"
#include "TraceMacros.h"
#include "SocketClient.h"

using namespace EndpointLog;

DataReader::DataReader(
    const std::shared_ptr<SocketClient> & sockClient,
    const std::shared_ptr<ConcurrentMap<LogItemPtr>> & dataCache
    ) :
    m_socketClient(sockClient),
    m_dataCache(dataCache)
{
    assert(m_socketClient);
}

DataReader::~DataReader()
{
    try {
        Stop();
    }
    catch(const std::exception & ex) {
        Log(TraceLevel::Error, "~DataReader() exception: " << ex.what());
    }
    catch(...) {
    } // no exception thrown from destructor
}

void
DataReader::Stop()
{
    ADD_INFO_TRACE;
    m_stopRead = true;
}

void
DataReader::Run()
{
    ADD_INFO_TRACE;

    try {
        std::string partialData;
        while(true) {
            if (!DoRead(partialData)) {
                break;
            }
        }
    }
    catch(const ReaderInterruptException&) {
        Log(TraceLevel::Info, "DataReader is interrupted. Abort reader thread.");
    }
    catch(const std::exception & ex) {
        Log(TraceLevel::Error, "DataReader unexpected exception: " << ex.what());
    }
}

bool
DataReader::DoRead(
    std::string & partialData
    )
{
    ADD_DEBUG_TRACE;

    char buf[512];
    try {
        InterruptPoint();
        auto readRtn = m_socketClient->Read(buf, sizeof(buf)-1);
        if (-1 == readRtn) {
            Log(TraceLevel::Debug, "SocketClient is stopped. Abort read.");
            return false;
        }

        if (readRtn > 0) {
            InterruptPoint();
            buf[readRtn] = '\0';
            partialData = ProcessData(partialData+buf);
            Log(TraceLevel::Debug, "DoRead partialData='" << partialData << "'.");
        }
    }
    catch(const SocketException & ex) {
        Log(TraceLevel::Info, "SocketException " << ex.what());
    }
    return true;
}

void
DataReader::InterruptPoint() const
{
    if (m_stopRead) {
        throw ReaderInterruptException();
    }
}

std::string
DataReader::ProcessData(
    const std::string & str
    )
{
    ADD_DEBUG_TRACE;

    Log(TraceLevel::Debug, "ProcessData: '" << str << "'.");

    if (str.empty()) {
        return std::string();
    }

    auto dPos = str.find_last_of('\n');
    if (std::string::npos == dPos) {
        return str;
    }

    std::istringstream iss(str);
    std::string item;
    while(std::getline(iss, item, '\n')) {
        if (!iss.eof()) {
            ProcessItem(item);
        }
    }

    if (!item.empty() && dPos == (str.size()-1)) {
        ProcessItem(item);
        return std::string();
    }

    return str.substr(dPos+1);
}

void
DataReader::ProcessItem(
    const std::string& item
    )
{
    ADD_DEBUG_TRACE;

    if (item.empty()) {
        Log(TraceLevel::Warning, "unexpected empty ack item found.");
        return;
    }

    m_nTagsRead++;
    Log(TraceLevel::Debug, "Got item='" << item << "'");

    auto p = item.find(':');
    if (p == std::string::npos) {
        ProcessTag(item);
    }
    else {
        auto tag = item.substr(0, p);
        auto ackStatus = item.substr(p+1);
        ProcessTag(tag, ackStatus);
    }
}

void
DataReader::ProcessTag(
    const std::string & tag
    )
{
    if (tag.empty()) {
        Log(TraceLevel::Warning, "unexpected empty tag found.");
        return;
    }

    if (m_dataCache) {
        if (1 != m_dataCache->Erase(tag)) {
            Log(TraceLevel::Warning, "tag '" << tag << "' is not found in backup cache");
        }
    }
}

static std::unordered_map<std::string, std::string> &
GetAckStatusMap()
{
    static std::unordered_map<std::string, std::string> m = 
    {
        { "0", "ACK_SUCCESS" },
        { "1", "ACK_FAILED" },
        { "2", "ACK_UNKNOWN_SCHEMA_ID" },
        { "3", "ACK_DECODE_ERROR" },
        { "4", "ACK_INVALID_SOURCE" },
        { "5", "ACK_DUPLICATE_SCHEMA_ID" }
    };
    return m;
}

static std::string
GetAckStatusStr(
    const std::string & ackCode
    )
{
    auto m = GetAckStatusMap();
    auto item = m.find(ackCode);
    if (item == m.end()) {
        return "Unknown-ACK-CODE";
    }
    return item->second;
}

void
DataReader::ProcessTag(
    const std::string & tag,
    const std::string & ackStatus
    )
{
    if (tag.empty()) {
        Log(TraceLevel::Warning, "unexpected empty tag found");
        return;
    }
    if (ackStatus.empty()) {
        Log(TraceLevel::Warning, "unexpected empty ack status string found");
        return;
    }

    if ("0" != ackStatus) {
        auto statusStr = GetAckStatusStr(ackStatus);
        Log(TraceLevel::Error, "unexpected mdsd ack status: " << statusStr);
    }

    if (m_dataCache) {
        if (1 != m_dataCache->Erase(tag)) {
            Log(TraceLevel::Warning, "tag '" << tag << "' is not found in backup cache");
        }
    }
}
