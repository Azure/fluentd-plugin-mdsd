#pragma once

#ifndef __ENDPOINTLOG_ILOGGER_H__
#define __ENDPOINTLOG_ILOGGER_H__

#include <string>

namespace EndpointLog {

class ILogger {
public:
    virtual void WriteLog(const std::string & msg) = 0;
};

} // namespace

#endif // __ENDPOINTLOG_ILOGGER_H__
