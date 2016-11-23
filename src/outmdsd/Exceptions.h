#pragma once

#ifndef __ENDPOINT_EXCEPTIONS__H__
#define __ENDPOINT_EXCEPTIONS__H__

#include <string>
#include <system_error>

namespace EndpointLog {

class SocketException : public std::system_error {
public:
    SocketException(int errnum, const std::string & msg) :
        std::system_error(errnum, std::system_category(), msg) {}

    SocketException(int errnum, const char * msg) :
        std::system_error(errnum, std::system_category(), msg) {}
};

class ReaderInterruptException {};

}

#endif // __ENDPOINT_EXCEPTIONS__H__
