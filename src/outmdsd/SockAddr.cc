#include <cstring>
#include <stdexcept>

extern "C" {
#include <arpa/inet.h>
}

#include "SockAddr.h"
#include "TraceMacros.h"
#include "Trace.h"

using namespace EndpointLog;

UnixSockAddr::UnixSockAddr(
    const std::string & socketfile
    )
{
    if (socketfile.empty()) {
        throw std::invalid_argument("UnixSockAddr: unexpected empty socketfile parameter.");
    }

    // maxLength: maximum permitted length of a path of a Unix domain socket
    constexpr auto maxLength = sizeof(addr.sun_path);
    if (socketfile.size() > maxLength) {
        throw std::invalid_argument("UnixSockAddr: socketfile '" + socketfile +
            "' exceeds max allowed length " + std::to_string(maxLength));
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socketfile.c_str(), maxLength);
    Log(TraceLevel::Info, "Create UNIX socket with '" << socketfile << "'");
}

TcpSockAddr::TcpSockAddr(
    int port
    )
{
    if (port <= 0) {
        throw std::invalid_argument("TcpSockAddr: invalid port " + std::to_string(port));
    }
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    Log(TraceLevel::Info, "Create IP socket with port=" << port);
}
