#include "outmdsd_log.h"
#include "Trace.h"
#include "FileTracer.h"

void
InitLogger(
    const std::string& logFilePath,
    bool createIfNotExist
)
{
    EndpointLog::Trace::SetTracer(new EndpointLog::FileTracer(logFilePath, createIfNotExist));
}

void
SetLogLevel(
    const std::string & level
)
{
    EndpointLog::Trace::SetTraceLevel(level);
}

