#include <unordered_map>

extern "C" {
#include <syslog.h>
}

#include "SyslogTracer.h"
#include "Trace.h"

namespace EndpointLog {

static std::unordered_map<TraceLevel, int, EnumClassHash>&
GetLevelTable()
{
    static std::unordered_map<TraceLevel, int, EnumClassHash> levelTable = {
        { TraceLevel::Trace, LOG_DEBUG },
        { TraceLevel::Debug, LOG_DEBUG },
        { TraceLevel::Info, LOG_INFO },
        { TraceLevel::Warning, LOG_WARNING },
        { TraceLevel::Error, LOG_ERR },
        { TraceLevel::Fatal, LOG_CRIT }
    };
    return levelTable;
}

static int
TraceLevel2SyslogLevel()
{
    auto traceLevel = Trace::GetTraceLevel();
    auto levelTable = GetLevelTable();
    auto iter = levelTable.find(traceLevel);
    if (iter != levelTable.end()) {
        return iter->second;
    }
    return LOG_DEBUG;
}

SyslogTracer::SyslogTracer(
    int option,
    int facility
    ) :
    m_logLevel(TraceLevel2SyslogLevel())
{
    openlog(NULL, option, facility);
}

SyslogTracer::~SyslogTracer()
{
    closelog();
}

void
SyslogTracer::WriteLog(
    const std::string& msg
    )
{
    syslog(m_logLevel, "%s", msg.c_str());
}


} // namespace
