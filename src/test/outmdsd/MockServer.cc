#include <iostream>
#include <stdexcept>
#include <system_error>

#include <vector>
#include <future>

extern "C" {
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <poll.h>
}

#include "MockServer.h"
#include "testutil.h"

using namespace TestUtil;

MockServer::~MockServer()
{
    Log("Call ~MockServer()");
    Stop();
}

void
MockServer::CloseServer()
{
    if (-1 != m_listenfd) {
        m_stopAccept = true;
        Log("shutdown and close listen fd " + std::to_string(m_listenfd));
        shutdown(m_listenfd, SHUT_RDWR);
        close(m_listenfd);
        m_listenfd = -1;
    }
    Log("called CloseServer()");
}

void
MockServer::CloseClients()
{
    std::lock_guard<std::mutex> lck(m_connMutex);
    for (const auto & fd : m_connfdSet) {
        Log("shutdown and close fd " + std::to_string(fd));
        shutdown(fd, SHUT_RDWR);
        close(fd);
    }
    m_connfdSet.clear();
    Log("called CloseClients()");
}

void
MockServer::ShutdownClients()
{
    std::lock_guard<std::mutex> lck(m_connMutex);
    for (const auto & fd : m_connfdSet) {
        Log("shutdown " + std::to_string(fd));
        shutdown(fd, SHUT_RDWR);
    }
}

void
MockServer::Stop()
{
    Log("call Stop()");
    m_stopFlag = true;
    m_stopAccept = true;
    CloseServer();
    CloseClients();
}

void
MockServer::ResetStopFlags()
{
    m_stopFlag = false;
    m_stopAccept = false;
}

void
MockServer::Init()
{
    RemoveFileIfExists(m_socketFile);

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, m_socketFile.c_str(), sizeof(addr.sun_path));

    m_listenfd = socket(AF_UNIX, SOCK_STREAM | O_NONBLOCK, 0);
    if (-1 == m_listenfd) {
        throw std::system_error(errno, std::system_category(), "socket() at " + m_socketFile);
    }

    Log("Start bind(). listenfd=" + std::to_string(m_listenfd) + " ...");
    // bind() will create the socket file
    if (-1 == bind(m_listenfd.load(), (struct sockaddr*) &addr, sizeof(addr))) {
        throw std::system_error(errno, std::system_category(),
            "bind(" + std::to_string(m_listenfd) + ")");
    }

    Log("Start listen() ...");
    if (-1 == listen(m_listenfd.load(), 10)) {
        throw std::system_error(errno, std::system_category(),
            "listen(" + std::to_string(m_listenfd) + ")");
    }
}

void
MockServer::Run()
{
    Log("Start Run() ...");
    int connfd = 0;
    m_runFinished = false;

    std::vector<std::future<void>> taskList;

    while(!m_stopAccept) {
        if (!PollConnection()) {
            Log("Error: Abort MockServer::Run()");
            break;
        }
        connfd = accept(m_listenfd.load(), NULL, 0);
        if (-1 == connfd) {
            if (EINTR == errno || EWOULDBLOCK == errno || EAGAIN == errno) {
                continue;
            }
            else {
                throw std::system_error(errno, std::system_category(),
                    "accept(" + std::to_string(m_listenfd) + ")");
            }
        }

        Log("accept() new connfd=" + std::to_string(connfd));

        auto task = std::async(std::launch::async, [this, connfd] () { RunEchoTask(connfd); });
        taskList.push_back(std::move(task));
    }

    Log("Wait tasks to finish. nTasks=" + std::to_string(taskList.size()));
    for (auto & t : taskList) {
        t.get();
    }
    Log("All tasks are done");

    std::lock_guard<std::mutex> lck(m_runMutex);
    m_runFinished = true;
    m_runCV.notify_one();
}

bool
MockServer::PollConnection() const
{
    Log("Start PollConnection() ...");
    struct pollfd pfds[1];
    pfds[0].fd = m_listenfd;
    pfds[0].events = POLLIN;

    int rtn = 0;
    while( (-1 == (rtn = poll(pfds, 1, -1))) && (EINTR == errno));
    auto errCopy = errno;
    if (rtn < 0) {
        std::error_code ec(errCopy, std::system_category());
        Log(std::string("poll() failed: ") + ec.message());
        return false;
    }

    if (pfds[0].revents & POLLHUP) {
        Log("poll() returned hang-up. Socket is closed.");
        return false;
    }
    if (!(pfds[0].revents & POLLIN)) {
        Log("Error: poll() returned unexpected event.");
        return false;
    }

    Log("Finished PollConnection() Successfully.");
    return true;
}

void
MockServer::RunEchoTask(
    int connfd
    )
{
    if (connfd <= 0) {
        Log("RunEchoTask(): invalid connection id. Abort.");
        return;
    }

    Log("RunEchoTask connfd=" + std::to_string(connfd));

    std::unique_lock<std::mutex> lck(m_connMutex);
    m_connfdSet.insert(connfd);
    lck.unlock();

    // Because RunEchoTask is called in an async task, it is better to handle
    // exceptions at task level. Otherwise, the exceptions won't show up in
    // master thread until task.get() is called, which could cause unexpected delays
    // at handling client communications.
    try {
        StartReadLoop(connfd);
    }
    catch(const std::exception& ex) {
        Log(std::string("Error: RunEchoTask() failed: ") + ex.what());

        std::unique_lock<std::mutex> lck(m_connMutex);
        m_connfdSet.erase(connfd);
        lck.unlock();

        shutdown(connfd, SHUT_RDWR);
        close(connfd);
    }
}

void
MockServer::StartReadLoop(
    int connfd
    )
{
    if (-1 == fcntl(connfd, F_SETFL, O_NONBLOCK)) {
        throw std::system_error(errno, std::system_category(),
            "fcntl() on fd=" + std::to_string(connfd) + " for O_NONBLOCK");
    }

    std::string lastStrForMsgId;
    std::string lastStrForData;

    while(!m_stopFlag) {
        char buf[4096];
        auto rtn = read(connfd, buf, sizeof(buf));
        auto errCopy = errno;
        if (-1 == rtn) {
            if (EINTR == errno || EWOULDBLOCK == errno || EAGAIN == errno) {
                continue;
            }
            else if (ECONNRESET == errno) {
                Log("Lost client connfd="  + std::to_string(connfd) + ". Abort.");
                break;
            }
            else {
                Log("read(" + std::to_string(connfd) + ") failed: " + TestUtil::GetErrnoStr(errCopy) + ". Abort");
                break;
            }
        }
        else if (0 == rtn) {
            Log("read(" + std::to_string(connfd) + ") reaches EOF.");
            break;
        }

        Log("read(" + std::to_string(connfd) + ") #bytes = " + std::to_string(rtn));
        m_totalBytesRead += rtn;
        auto bufstr = std::string(buf, rtn);

        if (ProcessReadData(connfd, bufstr, lastStrForMsgId, lastStrForData)) {
            break;
        }
    }

    Log("RunEchoTask finished. connfd=" + std::to_string(connfd) +
        "; TotalRead(B)=" + std::to_string(m_totalBytesRead));
}

bool
MockServer::ProcessReadData(
    int connfd,
    const std::string & bufstr,
    std::string & lastStrForMsgId,
    std::string & lastStrForData
    )
{
    if (m_parseReadData) {
        GetMsgIdInfo(connfd, bufstr, lastStrForMsgId);

        if (GetMsgDataInfo(bufstr, lastStrForData)) {
            MarkTestDone();
            return true;
        }
    }
    else {
        lastStrForData += bufstr;
        if (IsTestDone(lastStrForData)) {
            MarkTestDone();
            return true;
        }

        auto lsz = lastStrForData.size();
        auto eotSize = TestUtil::EndOfTest().size();
        if (lsz > eotSize) {
            lastStrForData = lastStrForData.substr(lsz-eotSize);
        }
    }
    return false;
}

void
MockServer::GetMsgIdInfo(
    int connfd,
    const std::string & bufstr,
    std::string & lastStrForMsgId
    )
{
    lastStrForMsgId += bufstr;
    std::string leftOverForMsgId;
    auto msgIds = ParseMsgIds(lastStrForMsgId, leftOverForMsgId);
    lastStrForMsgId = std::move(leftOverForMsgId);

    if (!msgIds.empty()) {
        WriteBack(connfd, msgIds);
    }
}

bool
MockServer::GetMsgDataInfo(
    const std::string & bufstr,
    std::string & lastStrForData
    )
{
    lastStrForData.append(bufstr);
    bool isTestDone = IsTestDone(lastStrForData);

    std::string leftoverForData;
    ParseData(lastStrForData, leftoverForData);
    lastStrForData = std::move(leftoverForData);

    return isTestDone;
}

void
MockServer::MarkTestDone()
{
    Log("Received " + TestUtil::EndOfTest());
    Log("Notify finish CV");
    std::lock_guard<std::mutex> lck(m_finishMutex);
    m_stopFlag = true;
    m_finishCV.notify_all();
}

bool
MockServer::IsTestDone(const std::string & data)
{
    return (data.find(TestUtil::EndOfTest()) != std::string::npos);
}

bool
MockServer::WaitForRunLoopToFinish(uint32_t timeoutMS)
{
    Log("Start WaitForRunLoopToFinish ...");
    std::unique_lock<std::mutex> lck(m_runMutex);
    m_runCV.wait_for(lck, std::chrono::milliseconds(timeoutMS),
                       [this]() { return m_runFinished.load(); });

    Log("Finished WaitForRunLoopToFinish. m_runFinished=" + std::to_string(m_runFinished));
    return m_runFinished;
}

bool
MockServer::WaitForTestsDone(uint32_t timeoutMS)
{
    Log("Start WaitForTestsDone: timeoutMS=" + std::to_string(timeoutMS));

    std::unique_lock<std::mutex> lck(m_finishMutex);
    auto status = m_finishCV.wait_for(lck, std::chrono::milliseconds(timeoutMS),
        [this]() { return m_stopFlag.load(); });

    return status;
}

void
MockServer::WriteBack(
    int fd,
    const std::string & msg
    )
{
    while(!m_stopFlag) {
        // use MSG_NOSIGNAL so that no SIGPIPE is created
        auto rtn = send(fd, msg.c_str(), msg.size(), MSG_NOSIGNAL);
        auto errCopy = errno;
        if (-1 == rtn) {
            if (EINTR == errno || EWOULDBLOCK == errno || EAGAIN == errno) {
                continue;
            }
            else {
                Log("send(" + std::to_string(fd) + ") failed: " +
                    TestUtil::GetErrnoStr(errCopy) + ". Abort");
            }
        }
        else {
            Log("Successfully send(" + std::to_string(fd) + ") buf='" + msg + "'.");
        }
        break;
    }
}

void
MockServer::DisconnectAndRun(
    uint32_t disconnectTimeMS
    )
{
    if (0 == disconnectTimeMS) {
        return;
    }

    Log("Start to DisconnectAndRun(). time(ms)=" + std::to_string(disconnectTimeMS));
    CloseServer();
    ShutdownClients();

    WaitForRunLoopToFinish(disconnectTimeMS);

    usleep(disconnectTimeMS*1000);

    ResetStopFlags();
    Init();
    Run();
    Log("Finished to DisconnectAndRun() ...");
}

void
MockServer::Log(
    const std::string & msg
    ) const
{
    auto now = TestUtil::GetTimeNow();
    auto tid = std::this_thread::get_id();
    std::cout << now << " Mock: Th: " << tid << " " << msg << std::endl;
}

std::string
MockServer::ProcessMsgId(const std::string & msgId)
{
    Log("Get new msgid: " + msgId);
    if (!msgId.empty()) {
        m_totalTags++;
        return msgId + ":0\n";
    }
    return std::string();
}

// Get a msg id and update lastPos.
// Return true if found one, false if no msgid is found.
static bool
GetMsgId(
    const std::string & msg,
    size_t & lastPos,
    std::string& msgId
    )
{
    auto nlPos = msg.find_first_of('\n', lastPos);
    if (std::string::npos == nlPos) {
        return false;
    }

    auto comma1 = msg.find_first_of(',', nlPos);
    if (std::string::npos == comma1) {
        return false;
    }

    auto comma2 = msg.find_first_of(',', comma1+1);
    if (std::string::npos == comma2) {
        return false;
    }

    msgId = msg.substr(comma1+1, comma2-comma1-1);
    lastPos = comma2;

    return true;
}

std::string
MockServer::ParseMsgIds(
    const std::string & msg,
    std::string& leftover
    )
{
    std::string msgIds;
    size_t lastPos = 0;
    size_t leftoverPos = 0;

    while(true) {
        leftoverPos = lastPos;

        std::string msgId;
        if (!GetMsgId(msg, lastPos, msgId)) {
            break;
        }

        msgIds += ProcessMsgId(msgId);
    }

    leftover = msg.substr(leftoverPos);

    return msgIds; 
}

bool
MockServer::GetMsgData(
    const std::string & msg,
    size_t & lastPos
    )
{
    auto openBracketPos = msg.find_first_of('[', lastPos);
    if (std::string::npos == openBracketPos) {
        return false;
    }

    auto comma1 = msg.find_first_of(',', openBracketPos+1);
    if (std::string::npos == comma1) {
        return false;
    }

    auto comma2 = msg.find_first_of(',', comma1+1);
    if (std::string::npos == comma2) {
        return false;
    }

    auto closeBracketPos = msg.find_first_of(']', comma2+1);
    if (std::string::npos == closeBracketPos) {
        return false;
    }

    auto msgData = msg.substr(comma2+1, closeBracketPos-comma2-1);
    Log("Get msgdata '" + msgData + "'");
    if (!msgData.empty()) {
        m_dataSet.insert(msgData);
    }

    lastPos = closeBracketPos+1;
    return true;
}

// A typical full msg looks like below
// 36:'33
// ["testSource",1,ABC]'

void
MockServer::ParseData(
    const std::string & msg,
    std::string & leftover
    )
{
    size_t lastPos = 0;
    size_t leftoverPos = 0;

    while(true) {
        leftoverPos = lastPos;
        if (!GetMsgData(msg, lastPos)) {
            break;
        }
    }
    leftover = msg.substr(leftoverPos);
}
