#pragma once

#ifndef __ITRACER_H__
#define __ITRACER_H__

#include <string>

namespace EndpointLog {

class ITracer {
public:
    virtual void WriteLog(const std::string & msg) = 0;
};

} // namespace

#endif // __ITRACER_H__
