extern "C" {
#include <unistd.h>
#include <sys/eventfd.h>
#include <poll.h>
#include <assert.h>
}

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

    CreateAbortSendEventFD();
    CreateAbortReadEventFD();
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
        Close();

        if (-1 != m_abortSendEventFD) {
            close(m_abortSendEventFD);
            m_abortSendEventFD = -1;
        }

        if (-1 != m_abortReadEventFD) {
            close(m_abortReadEventFD);
            m_abortReadEventFD = -1;
        }
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

        AbortBlockingPoll(m_abortSendEventFD);
        AbortBlockingPoll(m_abortReadEventFD);
    }
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

    auto sockRtn = socket(m_sockaddr->GetDomain(), SOCK_STREAM, 0);
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
        Log(TraceLevel::Debug, "close sockfd=" << m_sockfd);
        close(m_sockfd);
        m_sockfd = INVALID_SOCKET;
    }
}

size_t
SocketClient::Read(void* buf, size_t count, int timeoutMS)
{
    ADD_TRACE_TRACE;
    assert(m_abortReadEventFD >= 0);

    if (!buf) {
        throw std::invalid_argument("SocketClient::Read(): NULL pointer for buffer");
    }
    if (0 == count) {
        throw std::invalid_argument("SocketClient::Read(): read count cannot be 0.");
    }
    if (m_stopClient) {
        return 0;
    }

    WaitForSocketToBeReady(timeoutMS);

    if (m_stopClient) {
        return 0;
    }

    ssize_t readRet = 0;

    try {
        // todo: handle m_sockfd change in other threads
        PollSocket(POLLIN, m_abortReadEventFD);

        while (-1 == (readRet = read(m_sockfd, buf, count)) && EINTR == errno) {}

        if (readRet < 0) {
            throw SocketException(errno, "SocketClient read()");
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
        throw std::invalid_argument("SocketClient SendData(): length cannot be 0.");
    }

    while(!m_stopClient && total < len) {
        PollSocket(POLLOUT, m_abortSendEventFD);
        // Because the default behavior for SIGPIPE signal is to terminate the process,
        // use MSG_NOSIGNAL so that no SIGPIPE signal is created on errors.
        while (-1 == (rtn = send(m_sockfd, ((char*)buf)+total, bytesleft, MSG_NOSIGNAL)) && EINTR == errno) {}
        if (-1 == rtn) {
            throw SocketException(errno, "socket send() failed");
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

    auto len = strlen(data);
    Log(TraceLevel::Trace, "send(" << m_sockfd << ") nbytes=" << len << "; data='" << data << "'.");
    Send(data, len);
}

void
SocketClient::Send(
    const void *buf,
    size_t len
    )
{
    ADD_TRACE_TRACE;

    if (!buf) {
        throw std::invalid_argument("SocketClient::Send(): unexpected NULL for input data");
    }
    if (0 == len) {
        throw std::invalid_argument("SocketClient::Send(): buf length cannot be 0.");
    }

    assert(m_abortSendEventFD >= 0);

    try {
        Connect();
        if (m_sockfd < 0) {
            throw SocketException(0, "SocketClient Send(): invalid sockfd " + std::to_string(m_sockfd));
        }

        std::lock_guard<std::mutex> lck(m_sendMutex);
        SendData(buf, len);
    }
    catch(const SocketException & ex) {
        Close();
        throw;
    }
}

void
SocketClient::CreateAbortSendEventFD()
{
    ADD_DEBUG_TRACE;
    m_abortSendEventFD = eventfd(0, 0);
    if (m_abortSendEventFD < 0) {
        throw std::system_error(errno, std::system_category(), "abort-send-poll eventfd()");
    }
    Log(TraceLevel::Debug, "Created abort-send-poll eventfd=" << m_abortSendEventFD);
}

void
SocketClient::CreateAbortReadEventFD()
{
    ADD_DEBUG_TRACE;
    m_abortReadEventFD = eventfd(0, 0);
    if (m_abortReadEventFD < 0) {
        throw std::system_error(errno, std::system_category(), "abort-read-poll eventfd()");
    }
    Log(TraceLevel::Debug, "Created abort-read-poll eventfd=" << m_abortReadEventFD);
}

void
SocketClient::PollSocket(short pollMode, int abortPollFd)
{
    ADD_DEBUG_TRACE;

    if (0 == pollMode) {
        throw std::invalid_argument("PollSocket(): pollMode cannot be 0.");
    }
    if (abortPollFd < 0) {
        throw std::invalid_argument("PollSocket(): invalid abort poll fd=" + std::to_string(abortPollFd));
    }
    if (m_sockfd < 0) {
        throw SocketException(0, "PollSocket(): invalid sockfd=" + std::to_string(m_sockfd));
    }

    // use 2 fds: the first one to get real data; the second one to get event data.
    // event data are used to abort the poll() and abort further reading.
    struct pollfd pfds[2];
    pfds[0].fd = m_sockfd;
    pfds[0].events = pollMode;
    pfds[1].fd = abortPollFd;
    pfds[1].events = POLLIN;

    Log(TraceLevel::Info, "start poll(). abortEventfd=" << abortPollFd << " ...");
    int pollerr = 0;
    while( (-1 == (pollerr = poll(pfds, 2, -1))) && (EINTR == errno));
    auto errCopy = errno;

    try {
        if (pollerr <= 0) {
            throw SocketException(errCopy, "poll()");
        }
        if (pfds[1].revents & POLLIN) {
            // event received to abort poll()
            throw ReaderInterruptException();
        }
        if (!(pfds[0].revents & pollMode)) {
            throw SocketException(errCopy, "poll() returned unexpected event.");
        }
        if (pfds[0].revents & POLLHUP) {
            throw SocketException(errCopy, "poll() returned hang-up. Socket was closed.");
        }
    }
    catch(const SocketException & ex) {
        Log(TraceLevel::Info, "PollSocket() finished: " << ex.what());
        throw;
    }
}

void
SocketClient::AbortBlockingPoll(int eventfd) noexcept
{
    ADD_DEBUG_TRACE;

    if (eventfd < 0) {
        return;
    }

    Log(TraceLevel::Debug, "AbortBlockingPoll(): eventfd=" << eventfd);

    // the buffer size to write must >= 8 bytes
    uint64_t data = 1;
    if (write(eventfd, &data, sizeof(data)) <= 0) {
        std::error_code ec(errno, std::system_category());
        Log(TraceLevel::Error, "write() to eventfd " << eventfd << " failed: " << ec.message());
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
}
