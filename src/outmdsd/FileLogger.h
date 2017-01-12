#pragma once

#ifndef __FILELOGGER_H__
#define __FILELOGGER_H__

#include <string>
#include "ILogger.h"

namespace EndpointLog {

class FileLogger : public ILogger {
public:
    FileLogger(const std::string& filepath, bool createIfNotExists);
    ~FileLogger() = default;

    void WriteLog(const std::string& msg);

private:
    std::string m_filepath;
    int m_fd;    
};

} // namespace

#endif // __FILELOGGER_H__
