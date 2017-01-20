#pragma once

#ifndef __SYSLOGTRACER_H__
#define __SYSLOGTRACER_H__

#include <string>
#include "ITracer.h"

namespace EndpointLog {

class SyslogTracer : public ITracer {
public:
    /// This class implements logging using syslog.
    /// It uses openlog(NULL, option, facility) for the logging.
    SyslogTracer(int option, int facility);
    ~SyslogTracer();

    void WriteLog(const std::string& msg);

private:
    int m_logLevel;
};

} // namespace

#endif // __SYSLOGTRACER_H__
