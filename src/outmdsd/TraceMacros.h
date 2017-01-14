#pragma once
#ifndef __TRACEMACROS_H__
#define __TRACEMACROS_H__

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

#endif
