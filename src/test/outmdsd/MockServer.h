#pragma once
#ifndef __MOCKSERVER_H__
#define __MOCKSERVER_H__

#include <string>
#include <atomic>
#include <unordered_set>
#include <mutex>
#include <condition_variable>

namespace TestUtil
{

/// This is a simple socket server for testing. It simulates design in mdsd to
/// listen for connection, set up connection, read data, and sent back TAG\n in
/// acknowldge.
///
class MockServer
{
public:
    MockServer(const std::string & socketFile, bool parseReadData = true) :
        m_socketFile(socketFile),
        m_parseReadData(parseReadData)
    {}

    ~MockServer();

    void Init();

    // start and run read loop. For each read(), write back a response string.
    // Return total bytes read.
    void Run();

    // Wait until all tests are done or timeout.
    // return true if success, false if timeout.
    bool WaitForTestsDone(uint32_t timeoutMS);

    // notify MockServer to stop
    void Stop();

    // Close connection and sockets. Wait for N milliseconds. Then
    // create new socket and connections.
    // This is to mock socket or connection failures.
    void DisconnectAndRun(uint32_t disconnectTimeMS);

    size_t GetTotalBytesRead() const { return m_totalBytesRead; }

    size_t GetTotalTags() const { return m_totalTags; }

    std::unordered_set<std::string> GetUniqDataRead() const { return m_dataSet; }

private:
    // poll socket server for new socket connection.
    // Return true if poll() returns successfully, return false if any error.
    bool PollConnection() const;
    void ResetStopFlags();
    void CloseServer();
    void CloseClients();
    void ShutdownClients();

    void RunEchoTask(int connfd);
    void StartReadLoop(int connfd);
    void Log(const std::string & msg) const;
    void WriteBack(int fd, const std::string & msg);

    // Process the data read from socket.
    // Return true if end of test is received, return false otherwise.
    bool ProcessReadData(int connfd, const std::string & bufstr,
        std::string & lastStrForMsgId, std::string & lastStrForData);

    void GetMsgIdInfo(int connfd, const std::string & bufstr, std::string & lastStrForMsgId);

    // Process the data read from socket and get data value info.
    // Return true if end of test is received, return false otherwise.
    bool GetMsgDataInfo(const std::string & bufstr, std::string & lastStrForData);

    std::string ParseMsgIds(const std::string & msg, std::string& leftover);
    std::string ProcessMsgId(const std::string & msgid);

    void ParseData(const std::string & msg, std::string & leftover);
    bool GetMsgData(const std::string & msg, size_t & lastPos);

    bool WaitForRunLoopToFinish(uint32_t timeoutMS);

    // When MockServer receives end-of-test msg, it marks the test is done.
    // Return true if done, return false otherwise.
    bool IsTestDone(const std::string & str);
    void MarkTestDone();

private:
    std::string m_socketFile;
    bool m_parseReadData = true; // if true, parse details from read data; if false, don't parse.

    std::atomic<bool> m_stopFlag{false}; // a flag to tell server to stop
    std::atomic<bool> m_stopAccept{false}; // stop accept any new connection

    std::atomic<int> m_listenfd{-1};

    std::atomic<size_t> m_totalBytesRead{0};  // total nbytes read by the server

    // a set for all unique data read in server. This only contains the 'data' part,
    // which is everything excluding the msgid. example
    // ["testSource",,ABC]'
    // Because of possible msg resent from client, this is used to remove duplicates.
    std::unordered_set<std::string> m_dataSet;

    // total number of tags read by the server. can have duplicates.
    std::atomic<size_t> m_totalTags{0};

    std::unordered_set<int> m_connfdSet; // To save all connect FDs.
    std::mutex m_connMutex; // to lock m_connfdSet

    // synchronize to finish all the tests
    std::mutex m_finishMutex;
    std::condition_variable m_finishCV;

    // synchronize after Run() is done for failure testing
    std::atomic<bool> m_runFinished{false};
    std::mutex m_runMutex;
    std::condition_variable m_runCV;
};

} // namespace

#endif // __MOCKSERVER_H__
