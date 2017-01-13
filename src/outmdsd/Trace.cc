#include <iomanip>
#include <cstring>
#include <iostream>

extern "C" {
#include <sys/time.h>
}

#include "Trace.h"
#include "Exceptions.h"
#include "FileTracer.h"
#include "SyslogTracer.h"

using namespace EndpointLog;

TraceLevel Trace::s_minLevel = TraceLevel::Info;
ITracer* Trace::s_logger = nullptr;

static
std::string GetFileBasename(
    const std::string & filepath
    )
{
    size_t p = filepath.find_last_of('/');
    if (p == std::string::npos) {
        return filepath;
    }
    return filepath.substr(p+1);
}

void
Trace::Init(
    const std::string & filepath,
    bool createIfNotExist
    )
{
    GetLevelStrTable();

    if (s_logger) {
        delete s_logger;
    }
    s_logger = new FileTracer(filepath, createIfNotExist);
}

void
Trace::Init(
    int syslogOption,
    int syslogFacility
    )
{
    GetLevelStrTable();

    if (s_logger) {
        delete s_logger;
    }
    s_logger = new SyslogTracer(syslogOption, syslogFacility);
}

std::string
GetTimeNow()
{
    struct timeval tv;
    (void) gettimeofday(&tv, 0);
    
    struct tm zulu;
    (void) gmtime_r(&(tv.tv_sec), &zulu);

    char buf[128];
    auto rtn = strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &zulu);
    if (0 == rtn) {
        throw std::runtime_error("strftime() failed");
    }
    auto usec = static_cast<unsigned long>(tv.tv_usec);
    std::ostringstream strm;
    strm << buf << "." << std::setfill('0') << std::setw(6) << usec << "0Z";
    return strm.str();
}


void
Trace::WriteLog(
    TraceLevel traceLevel,
    const std::string & msg,
    const char * filename,
    int lineNumber
    )
{
    if (traceLevel < s_minLevel) {
        return;
    }
    
    try {
        std::string levelStr = TraceLevel2Str(traceLevel);
        std::string basename = GetFileBasename(filename);
        
        auto now = GetTimeNow();
        
        char buf[1024];
        snprintf(
                 buf,
                 sizeof(buf),
                 "%s: %s %s:%d %s\n",
                 now.c_str(),
                 levelStr.c_str(),
                 basename.c_str(),
                 lineNumber,
                 msg.c_str());

        s_logger->WriteLog(buf);
    }
    catch(const std::exception & ex) {
        std::cout << "Error: Trace::WriteLog() failed: " << ex.what() << std::endl;
    }
}

std::string
Trace::TraceLevel2Str(TraceLevel level) noexcept
{
    auto levelTable = GetLevelStrTable();
    auto iter = levelTable.find(level);
    if (iter != levelTable.end()) {
        return iter->second;
    }
    return std::string("TraceLevel_" + std::to_string(static_cast<int>(level)));
}

TraceLevel
Trace::TraceLevelFromStr(const std::string & level) noexcept
{
    auto t = GetStr2LevelTable();
    auto iter = t.find(level);
    if (iter != t.end()) {
        return iter->second;
    }
    return TraceLevel::Warning;
}

std::unordered_map<TraceLevel, std::string, EnumClassHash>&
Trace::GetLevelStrTable()
{
    static std::unordered_map<TraceLevel, std::string, EnumClassHash> levelTable =  {
        { TraceLevel::Trace, "trace" },
        { TraceLevel::Debug, "debug" },
        { TraceLevel::Info, "info" },
        { TraceLevel::Warning, "warn" },
        { TraceLevel::Error, "error" },
        { TraceLevel::Fatal, "fatal" }
    };
    return levelTable;
}

// Get string to level enum table
std::unordered_map<std::string, TraceLevel>&
Trace::GetStr2LevelTable()
{
    static std::unordered_map<std::string, TraceLevel> t =  {
        { "trace", TraceLevel::Trace },
        { "debug", TraceLevel::Debug },
        { "info", TraceLevel::Info },
        { "warn", TraceLevel::Warning },
        { "error", TraceLevel::Error },
        { "fatal", TraceLevel::Fatal }
    };
    return t;
}
