extern "C" {
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <assert.h>
}


#include <algorithm>
#include "SocketClient.h"
#include "SockAddr.h"
#include "Exceptions.h"
#include "Trace.h"


using namespace EndpointLog;

SocketClient::SocketClient(
    const std::string & socketfile,
    int connRetryTimeoutMS
    ) :
    m_sockaddr(std::make_shared<UnixSockAddr>(socketfile)),
    m_connRetryTimeoutMS(connRetryTimeoutMS)
{
    m_lastConnTime.tv_sec = 0;
    m_lastConnTime.tv_usec = 0;
}

SocketClient::SocketClient(
    int port,
    int connRetryTimeoutMS
    ) :
    m_sockaddr(std::make_shared<TcpSockAddr>(port)),
    m_connRetryTimeoutMS(connRetryTimeoutMS)
{
    m_lastConnTime.tv_sec = 0;
    m_lastConnTime.tv_usec = 0;
}

SocketClient::~SocketClient()
{
    try {
        Stop();
    }
    catch(const std::exception & ex) {
        Log(TraceLevel::Error, "~SocketClient() exception: " << ex.what());
    }
    catch(...) {
    } // no exception thrown from destructor
}

void
SocketClient::Stop()
{
    if (!m_stopClient) {
        Log(TraceLevel::Debug, "SocketClient::Stop()");

        std::unique_lock<std::mutex> connlk(m_fdMutex);
        m_stopClient = true;
        m_connCV.notify_all();
        connlk.unlock();
    }
    Close();
}

void
SocketClient::SetupSocketConnect()
{
    ADD_DEBUG_TRACE;

    if (INVALID_SOCKET != m_sockfd) {
        return;
    }

    (void) gettimeofday(&m_lastConnTime, 0);
    m_numConnect++;

    auto sockRtn = socket(m_sockaddr->GetDomain(), SOCK_STREAM | O_NONBLOCK, 0);
    if (-1 == sockRtn) {
        throw SocketException(errno, "SocketClient socket()");
    }
    m_sockfd = sockRtn;

    if (-1 == connect(m_sockfd, m_sockaddr->GetAddress(), m_sockaddr->GetAddrLen())) {
        close(m_sockfd);
        m_sockfd = INVALID_SOCKET;
        throw SocketException(errno, "SocketClient connect()");
    }

    Log(TraceLevel::Debug, "Successfully connect() to sockfd=" << m_sockfd);
}

static int
GetDiffMilliSeconds(
    const struct timeval& from,
    const struct timeval& to)
{
    return ( (to.tv_sec - from.tv_sec) * 1000 + (to.tv_usec -  from.tv_usec) / 1000 );
}

static int
GetRuntimeInMS(
    const struct timeval & startTime
    )
{
    struct timeval currTime;
    (void) gettimeofday(&currTime, 0);
    auto runtimeMS = GetDiffMilliSeconds(startTime, currTime);
    assert(runtimeMS >= 0);
    return runtimeMS;
}

bool
SocketClient::IsRetryTimeout(
    const struct timeval & startTime
    ) const
{
    auto runtimeMS = GetRuntimeInMS(startTime);
    if (runtimeMS >= m_connRetryTimeoutMS) {
        Log(TraceLevel::Error, "Connect() timeout after " << m_connRetryTimeoutMS
            << " ms. Stop retrying.");
        return true;
    }
    return false;
}

void
SocketClient::Connect()
{
    ADD_TRACE_TRACE;
    if (INVALID_SOCKET != m_sockfd) {
        return;
    }

    struct timeval startTime;
    (void) gettimeofday(&startTime, 0);

    std::lock_guard<std::mutex> lock(m_fdMutex);

    while(!m_stopClient) {
        try {
            SetupSocketConnect();
            m_connCV.notify_all();
            break;
        }
        catch(const SocketException & ex) {
            Log(TraceLevel::Error, "Connect() SocketException: " << ex.what());

            auto runtimeMS = GetRuntimeInMS(startTime);
            if (runtimeMS >= m_connRetryTimeoutMS) {
                Log(TraceLevel::Error, "Connect() timeout after " << m_connRetryTimeoutMS
                    << " ms. Stop retrying.");
                break;
            }

            WaitBeforeReConnect(m_connRetryTimeoutMS-runtimeMS);

            if (IsRetryTimeout(startTime)) {
                break;
            }
        }
    }
}

void
SocketClient::Close()
{
    ADD_DEBUG_TRACE;
    std::lock_guard<std::mutex> lock(m_fdMutex);

    if (INVALID_SOCKET != m_sockfd) {
        Log(TraceLevel::Debug, "shutdown and close sockfd=" << m_sockfd);
        // In multi-threads, poll() can get shutdown() event, but poll()
        // may not get close() event. see man 2 select.
        // shutdown should use SHUT_RDWR mode because PollSocket() can do
        // either POLLIN or POLLOUT.
        shutdown(m_sockfd, SHUT_RDWR);
        close(m_sockfd);
        m_sockfd = INVALID_SOCKET;
    }
}

ssize_t
SocketClient::Read(void* buf, size_t count, int timeoutMS)
{
    ADD_TRACE_TRACE;

    if (!buf) {
        throw std::invalid_argument("SocketClient::Read(): NULL pointer for buffer");
    }
    if (0 == count) {
        throw std::invalid_argument("SocketClient::Read(): read count cannot be 0.");
    }
    if (m_stopClient) {
        Log(TraceLevel::Debug, "SocketClient is already stopped.");
        return -1;
    }

    WaitForSocketToBeReady(timeoutMS);

    if (m_stopClient) {
        Log(TraceLevel::Debug, "SocketClient is already stopped.");
        return -1;
    }

    ssize_t readRet = 0;

    try {
        PollSocket(POLLIN);

        while (-1 == (readRet = read(m_sockfd, buf, count)) && EINTR == errno) {}
        if (readRet < 0) {
            if (EAGAIN == errno || EWOULDBLOCK == errno) {
                Log(TraceLevel::Trace, "SocketClient retry read()");
                readRet = 0;
            }
            else {
                throw SocketException(errno, "SocketClient read()");
            }
        }
        else if (0 == readRet) {
            // when the other socket side connection is closed, read() returns 0.
            // close client side when this occurs.
            Close();
        }
    }
    catch(const SocketException & ex) {
        Close();
        throw;
    }

    Log(TraceLevel::Trace, "read() returned nbytes=" << readRet);
    return static_cast<size_t>(readRet);
}

// Send a buffer through socket. It handles partial send().
void
SocketClient::SendData(
    const void *buf,
    size_t len
    )
{
    ADD_TRACE_TRACE;

    size_t total = 0;        // how many bytes we've sent
    size_t bytesleft = len;
    ssize_t rtn = 0;

    if (m_sockfd < 0) {
        throw SocketException(0, "SocketClient SendData(): invalid sockfd " + std::to_string(m_sockfd));
    }
    if (!buf) {
        throw std::invalid_argument("SocketClient SendData(): unexpected NULL for buf pointer.");
    }
    if (0 == len) {
        return;
    }

    std::lock_guard<std::mutex> lck(m_sendMutex);
    while(!m_stopClient && bytesleft) {
        PollSocket(POLLOUT);
        // Because the default behavior for SIGPIPE signal is to terminate the process,
        // use MSG_NOSIGNAL so that no SIGPIPE signal is created on errors.
        while (-1 == (rtn = send(m_sockfd, ((char*)buf)+total, bytesleft, MSG_NOSIGNAL)) && EINTR == errno) {}
        if (-1 == rtn) {
            if (EAGAIN == errno || EWOULDBLOCK == errno) {
                // There may be circumstances in which a file descriptor is spuriously reported as ready.
                // Just retry when this occurs. see man 2 poll, man 2 select.
                continue;
            }
            else {
                throw SocketException(errno, "socket send()");
            }
        }

        Log(TraceLevel::Trace, "sent (" << m_sockfd << ") nbytes=" << rtn);

        total += rtn;
        bytesleft -= rtn;
    }
}

void
SocketClient::Send(
    const char* data
    )
{
    ADD_TRACE_TRACE;

    if (!data) {
        throw std::invalid_argument("SocketClient::Send(): unexpected NULL for input data");
    }

    Send(data, strlen(data));
}

void
SocketClient::Send(
    const void *buf,
    size_t len
    )
{
    ADD_TRACE_TRACE;

    if (0 == len) {
        return;
    }

    if (!buf) {
        throw std::invalid_argument("SocketClient::Send(): unexpected NULL for input data");
    }

    try {
        Connect();
        if (m_sockfd < 0) {
            throw SocketException(0, "SocketClient Send(): invalid sockfd " + std::to_string(m_sockfd));
        }

        SendData(buf, len);
    }
    catch(const SocketException & ex) {
        Close();
        throw;
    }
}

void
SocketClient::PollSocket(short pollMode)
{
    ADD_DEBUG_TRACE;

    if (0 == pollMode) {
        throw std::invalid_argument("PollSocket(): pollMode cannot be 0.");
    }

    if (m_sockfd < 0) {
        throw SocketException(0, "PollSocket(): invalid sockfd=" + std::to_string(m_sockfd));
    }

    struct pollfd pfds[1];
    pfds[0].fd = m_sockfd;
    pfds[0].events = pollMode;

    Log(TraceLevel::Info, "start poll() ...");
    int pollRtn = 0;
    while( (-1 == (pollRtn = poll(pfds, 1, -1))) && (EINTR == errno));
    auto errCopy = errno;

    try {
        if (pollRtn < 0) {
            throw SocketException(errCopy, "poll()");
        }
        if (pfds[0].revents & POLLHUP) {
            throw SocketException(errCopy, "poll() returned hang-up. Socket was closed.");
        }
        if (!(pfds[0].revents & pollMode)) {
            throw SocketException(errCopy, "poll() returned unexpected event.");
        }
    }
    catch(const SocketException & ex) {
        Log(TraceLevel::Info, "PollSocket() finished: " << ex.what());
        throw;
    }
}

void
SocketClient::WaitBeforeReConnect(
    int maxWaitMS
    )
{
    ADD_DEBUG_TRACE;

    const int minDelay = 100; // ms
    auto maxDelay = std::min(60000, maxWaitMS);

    auto k = 1 << (m_numConnect % 10);
    auto delayMS = std::min(minDelay * k, maxDelay);

    Log(TraceLevel::Trace, "WaitBeforeReConnect (ms): " << delayMS);

    int ntimes = delayMS/minDelay;
    for (int i = 0; i < ntimes && !m_stopClient; i++) {
        usleep(minDelay*1000);
    }
    auto leftMS = delayMS % minDelay;
    if (!m_stopClient && leftMS > 0) {
        usleep(leftMS*1000);
    }
}

void
SocketClient::WaitForSocketToBeReady(
    int timeoutMS
    )
{
    ADD_TRACE_TRACE;

    std::unique_lock<std::mutex> connlk(m_fdMutex);
    m_connCV.wait_for(connlk, std::chrono::milliseconds(timeoutMS), [this] { 
        return (m_stopClient || INVALID_SOCKET != m_sockfd); 
    });

    if (m_stopClient) {
        return;
    }
    if (INVALID_SOCKET == m_sockfd) {
        throw SocketException(0, "WaitForSocketToBeReady: socket fd is invalid");
    }
}
