#include "outmdsd_log.h"
#include "Trace.h"

void
InitLogger(
    const std::string& logFilePath,
    bool createIfNotExist
)
{
    EndpointLog::Trace::Init(logFilePath, createIfNotExist);
}

void
SetLogLevel(
    const std::string & level
)
{
    EndpointLog::Trace::SetTraceLevel(level);
}

