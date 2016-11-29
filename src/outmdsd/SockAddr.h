#pragma once
#ifndef __ENDPOINT_SOCKADDR_H__
#define __ENDPOINT_SOCKADDR_H__

extern "C" {
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
}
#include <string>

namespace EndpointLog {

// This class implements APIs for socket() programming.
class SockAddr {
public:
    virtual ~SockAddr() {}

    /// Return socket communication domain used in socket().
    virtual int GetDomain() const = 0;

    /// Return socket address used in connect().
    virtual struct sockaddr* GetAddress() const = 0;

    /// Return size of a socket address used in connect().
    virtual int GetAddrLen() const = 0;
};

class UnixSockAddr : public SockAddr {
public:
    UnixSockAddr(const std::string & socketfile);

    int GetDomain() const { return AF_UNIX; }

    struct sockaddr* GetAddress() const {
        return (struct sockaddr*)(&addr);
    }

    int GetAddrLen() const { return sizeof(addr); }

private:
    struct sockaddr_un addr;
};

class TcpSockAddr : public SockAddr {
public:
    TcpSockAddr(int port);

    int GetDomain() const { return AF_INET; }

    struct sockaddr* GetAddress() const {
        return (struct sockaddr*)(&addr);
    }

    int GetAddrLen() const { return sizeof(addr); }

private:
    struct sockaddr_in addr;
};

} // namespace

#endif // __ENDPOINT_SOCKADDR_H__