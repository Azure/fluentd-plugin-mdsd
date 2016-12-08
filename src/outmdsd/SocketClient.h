#pragma once
#ifndef __ENDPOINT_SOCKETCLIENT_H__
#define __ENDPOINT_SOCKETCLIENT_H__

#include <string>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>

extern "C" {
#include <sys/time.h>
}

namespace EndpointLog {

class SockAddr;

/// This is a specialized class to do socket send/read for the following scenario:
/// - The socket server side may lose connection at any time (e.g. server process reboots).
/// - Multiple threads sharing same connection can do send and read concurrently.
/// - All the threads can be stopped immediately if needed when they are calling this class.
///
/// It handles connection failure before each send(). If the connection
/// is invalid, it will try to create new connection with certain delay
/// between retries. The read() operation will block until timed out if there is
/// connection failure before actual read() is performed.
///
class SocketClient {
public:
    /// <summary>
    /// Construct a new object using Unix domain socket file.
    /// </summary>
    /// <param name="socketfile">unix domain socket file</param>
    /// <param name="connRetryTimeoutMS">number of milli-seconds to timeout socket
    /// connect() retry </param>
    SocketClient(const std::string & socketfile, int connRetryTimeoutMS=60*1000);

    /// <summary>
    /// Construct a new object using TCP/IP port.
    /// </summary>
    /// <param name="port">port number</param>
    /// <param name="connRetryTimeoutMS">number of milli-seconds to timeout socket
    /// connect() retry </param>
    SocketClient(int port, int connRetryTimeoutMS=60*1000);

    ~SocketClient();

    /// <summary> Notice SocketClient to stop further operation </summary>
    void Stop();

    /// <summary>
    /// Create new socket and connect to it if the sock fd is invalid.
    /// If connection fails, socket will be closed.
    /// Throw exception if any error.
    /// </summary>
    void Connect();

    /// <summary>Get number of connect() is called. It is for testing only</summary>
    size_t GetNumReConnect() const { return m_numConnect; }

    /// <summary>
    /// Read up to bufsize from the socket fd and save data to given buffer.
    /// If the socket fd is not valid, wait until timeout.
    /// Socket will be closed if any socket error is found, or EOF is reached.
    /// Throw exception for any error.
    /// Return number of bytes read. or -1 if the SocketClient is already stopped.
    /// </summary>
    /// <param name='buf'>buffer where data will be saved. The caller must make sure 
    /// that 'buf' is valid and  it has at least bufsize space. </param>
    /// <param name='count'>max number of bytes to read</param>
    /// <param name='timeoutMS'>milli-seconds to wait before timeout if socket fd
    /// is invalid.</param>
    ssize_t Read(void* buf, size_t count, int timeoutMS = 60*1000);

    /// <summary>
    /// Send a data buffer to the socket. If 'len' is 0, do nothing.
    /// The caller must make sure 'buf' is valid.
    ///
    /// If socket is not connected, Send() will try in a loop to set up connection.
    /// The max time trying to set up the connection is connRetryTimeoutMS.
    ///
    /// if Send() fails at runtime, socket will be closed.
    /// Throw exception for any error.
    ///</summary>
    void Send(const void* buf, size_t len);

    /// <summary>
    /// Send data string to the socket. Send all the data until a terminal NUL is hit.
    /// The NUL char is not sent.
    /// If string is empty, do nothing.
    /// Throw exception for any error.
    /// </summary>
    void Send(const char* data);

    /// close socket fd.
    void Close();

private:
    void SetupSocketConnect();

    /// Return true if the time from 'startTime' to 'now' is bigger or equal to
    /// m_connRetryTimeoutMS. Return false otherwise.
    bool IsRetryTimeout(const struct timeval & startTime) const;

    /// Wait some time using exponential delay policy before next connect() retry.
    /// <param name="maxWaitMS"> max milli-seconds to wait</param>
    void WaitBeforeReConnect(int maxWaitMS);

    /// Wait until socket fd is a valid number, or until timed out.
    /// <param name="timeoutMS"> max milli-seconds to wait</param>
    void WaitForSocketToBeReady(int timeoutMS);

    /// send data to the socket server.
    /// <param name='data'> data buffer. The caller must make sure it is valid and has
    /// dataLen size.</param>
    /// <param name='dataLen'> number of bytes to send </param>
    void SendData(const void* data, size_t dataLen);

    /// <summary>
    /// poll() on the sock fd for I/O.
    /// Use a abortPollFd to abort waiting poll() when needed.
    /// Throw exception for any error.
    /// </summary>
    /// <param name="pollMode"> poll() events value. e.g. POLLIN, POLLOUT, etc</param>
    void PollSocket(short pollMode);

private:
    constexpr static int INVALID_SOCKET = -1;
    std::shared_ptr<SockAddr> m_sockaddr;
    int m_connRetryTimeoutMS = 0;  // milli-seconds to timeout connect() retry.

    std::atomic<int> m_sockfd{INVALID_SOCKET};
    std::mutex m_fdMutex;  // protect sockfd at socket creation/close time.
    std::mutex m_sendMutex; // avoid interleaved message in multi Send() at the same time.

    struct timeval m_lastConnTime; // last time to create a new socket. used for delay policy.

    // SocketClient has several APIs that'll block for certain things to be ready. For example,
    // poll() for I/O, and Read() for valid socket fd, m_stopClient is used
    // to make sure these blocking APIs can be interrupted immediately.
    std::atomic<bool> m_stopClient{false}; // atomic flag to stop further operations.

    // signal when sock fd is ready to use. This is used when some thread(s) are waiting
    // for the sock fd to be ready (e.g. Read() API).
    std::condition_variable m_connCV;

    size_t m_numConnect = 0; // number of times to create a new socket.
};

} // namespace

#endif // __ENDPOINT_SOCKETCLIENT_H__
