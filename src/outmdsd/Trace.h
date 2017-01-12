#pragma once

#ifndef __ENDPOINT_TRACE_H__
#define __ENDPOINT_TRACE_H__

#include <string>
#include <sstream>
#include <unordered_map>

#define ADD_INFO_TRACE \
    Trace _trace(TraceLevel::Info, __func__, __FILE__, __LINE__)
/**/
#define ADD_DEBUG_TRACE \
    Trace _trace(TraceLevel::Debug, __func__, __FILE__, __LINE__)
/**/
#define ADD_TRACE_TRACE \
    Trace _trace(TraceLevel::Trace, __func__, __FILE__, __LINE__)
/**/
#define Log(level, message) \
    if (level >= Trace::GetTraceLevel()) { \
        std::ostringstream _ss; \
        _ss << message; \
        Trace::WriteLog(level, _ss.str(), __FILE__, __LINE__); \
    } \
/**/

namespace EndpointLog {
    enum class TraceLevel {
        Trace,
        Debug,
        Info,
        Warning,
        Error,
        Fatal
    };

    struct EnumClassHash {
        template <typename T>
        size_t operator()(T t) const {
            return static_cast<size_t>(t);
        }
    };

    class Trace {
    public:
        Trace(
            TraceLevel level,
            const std::string & func,
            const char* srcFilename,
            int lineNumber) :
            m_level(level),
            m_func(func),
            m_srcFilename(srcFilename),
            m_lineNumber(lineNumber)
        {
            if (level >= s_minLevel) {
                std::ostringstream ss;
                ss << "Entering " << m_func;
                WriteLog(level, ss.str(), srcFilename, lineNumber);
            }
        }

        Trace(
            TraceLevel level,
            std::string && func,
            const char* srcFilename,
            int lineNumber) :
            m_level(level),
            m_func(std::move(func)),
            m_srcFilename(srcFilename),
            m_lineNumber(lineNumber)
        {
            if (level >= s_minLevel) {
                std::ostringstream ss;
                ss << "Entering " << m_func;
                WriteLog(level, ss.str(), srcFilename, lineNumber);
            }
        }

        ~Trace()
        {
            if (m_level >= s_minLevel) {
                std::ostringstream ss;
                ss << "Leaving " << m_func;
                WriteLog(m_level, ss.str(), m_srcFilename, m_lineNumber);
            }
        }

        /// Initialize tracing. This function must be called before doing any tracing.
        /// Throw exceptions if any error.
        /// <param name="logFilePath"> log file path </param>
        /// <param name="createIfNotExist"> If true, create the log file if it
        /// exist. If false, assume the file already exists. </param>
        static void Init(const std::string& logFilePath, bool createIfNotExist);

        static void SetTraceLevel(TraceLevel level)
        {
            s_minLevel = level;
        }

        static void SetTraceLevel(const std::string & level)
        {
            s_minLevel = TraceLevelFromStr(level);
        }

        static TraceLevel GetTraceLevel() {
            return s_minLevel;
        }

        static void WriteLog(TraceLevel level, const std::string & msg,
                             const char* filename, int lineNumber);

    private:
        TraceLevel m_level;
        std::string m_func;
        const char* m_srcFilename;
        int m_lineNumber;

        static TraceLevel s_minLevel;
        static class ILogger* s_logger;

        static std::unordered_map<TraceLevel, std::string, EnumClassHash>& GetLevelStrTable();
        static std::unordered_map<std::string, TraceLevel>& GetStr2LevelTable();

        static std::string TraceLevel2Str(TraceLevel level) noexcept;
        static TraceLevel TraceLevelFromStr(const std::string & level) noexcept;
    };

}

#endif // __ENDPOINT_TRACE_H__
