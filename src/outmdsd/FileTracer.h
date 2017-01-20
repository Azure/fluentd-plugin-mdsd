#pragma once

#ifndef __FILETRACER_H__
#define __FILETRACER_H__

#include <string>
#include "ITracer.h"

namespace EndpointLog {

class FileTracer : public ITracer {
public:
    FileTracer(const std::string& filepath, bool createIfNotExists);
    ~FileTracer() = default;

    void WriteLog(const std::string& msg);

private:
    std::string m_filepath;
    int m_fd;    
};

} // namespace

#endif // __FILETRACER_H__
