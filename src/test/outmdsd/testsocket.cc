#include <boost/test/unit_test.hpp>
#include <future>
#include <sstream>

#include "MockServer.h"
#include "SocketClient.h"
#include "Exceptions.h"
#include "DataReader.h"
#include "DataResender.h"
#include "testutil.h"
#include "DjsonLogItem.h"
#include "CounterCV.h"

using namespace EndpointLog;

BOOST_AUTO_TEST_SUITE(testsocket)

BOOST_AUTO_TEST_CASE(Test_SocketClient_Send)
{
    try {
        const std::string socketfile = "/tmp/nosuchfile";
        SocketClient client(socketfile, 1);

        client.Connect();

        const char* testData = "SocketClient test data";
        BOOST_CHECK_THROW(client.Send(testData), SocketException);
    }
    catch(const std::exception & ex) {
        BOOST_FAIL("Test exception " << ex.what());
    }
}

// Test Read():
// it should wait for the connection to be ready until timeout;
// when socket is not connected, it should throw SocketException
BOOST_AUTO_TEST_CASE(Test_SocketClient_Read)
{
    try {
        SocketClient sc("/tmp/nosuchfile", 1);
        char buf[64];

        const int timeoutMS = 10;
        auto startTime = std::chrono::system_clock::now();
        BOOST_CHECK_THROW(sc.Read(buf, sizeof(buf), timeoutMS), SocketException);
        auto endTime = std::chrono::system_clock::now();
        auto runTimeMS = static_cast<uint32_t>((endTime-startTime)/std::chrono::milliseconds(1));
        BOOST_CHECK_GE(runTimeMS, timeoutMS);
    }
    catch(const std::exception & ex) {
        BOOST_FAIL("Test exception " << ex.what());
    }
}

/// Test Send() when socket connection fails
/// - Send() should throw expected exception
/// - Send() should timeout at expected time
BOOST_AUTO_TEST_CASE(Test_SocketClient_Reconnect_Error)
{
    try {
        const std::string socketfile = "/tmp/nosuchfile";
        const int timeoutMS = 10;
        SocketClient client(socketfile, timeoutMS);

        const char* testData = "SocketClient test data";
        int ntimes = 4;
        for (int i = 0; i < ntimes; i++) {
            auto startTime = std::chrono::system_clock::now();
            BOOST_CHECK_THROW(client.Send(testData), SocketException);
            auto endTime = std::chrono::system_clock::now();
            auto runTimeMS = static_cast<uint32_t>((endTime-startTime)/std::chrono::milliseconds(1));

            BOOST_CHECK_MESSAGE((runTimeMS >= timeoutMS), "Send() index=" << i);
        }

        BOOST_CHECK_LE(ntimes, client.GetNumReConnect());
    }
    catch(const std::exception & ex) {
        BOOST_FAIL("Test exception " << ex.what());
    }
}

static void
SendDataToServer(
    int nmsgs,
    size_t nbytes,
    bool insertNullChar,
    int32_t maxRunTimeMS
    )
{
    try {
        const std::string sockfile = TestUtil::GetCurrDir() + "/sockclient-bvt";
        auto mockServer = std::make_shared<TestUtil::MockServer>(sockfile, false);
        mockServer->Init();

        auto serverTask = std::async(std::launch::async, [mockServer]() {
            mockServer->Run();
            return mockServer->GetTotalBytesRead();
        });

        SocketClient client(sockfile, 1);

        char* buf = new char[nbytes];
        memset(buf, 'A', nbytes);

        if (insertNullChar && nbytes > 1) {
            buf[0] = '\0';
        }

        size_t totalSend = nbytes * nmsgs + TestUtil::EndOfTest().size();

        for (int i = 0; i < nmsgs; i++) {
            client.Send(buf, nbytes);
        }
        client.Send(TestUtil::EndOfTest().c_str());
        delete [] buf;

        bool mockServerDone = mockServer->WaitForTestsDone(maxRunTimeMS);
        BOOST_CHECK_EQUAL(true, mockServerDone);

        client.Stop();
        client.Close();
        mockServer->Stop();
        auto totalReceived = serverTask.get();

        BOOST_CHECK_EQUAL(totalSend, totalReceived);
    }
    catch(const std::exception & ex) {
        BOOST_FAIL("Test exception " << ex.what());
    }    
}

BOOST_AUTO_TEST_CASE(Test_SocketClient_Msg_Empty)
{
    SendDataToServer(10, 0, false, 100);
}

BOOST_AUTO_TEST_CASE(Test_SocketClient_Msg_1)
{
    SendDataToServer(1, 16, false, 100);
}

BOOST_AUTO_TEST_CASE(Test_SocketClient_Msg_NullChar)
{
    SendDataToServer(10, 33, true, 100);
}

BOOST_AUTO_TEST_CASE(Test_SocketClient_Msg_1M)
{
    SendDataToServer(10, 1024*1024, false, 500);
}

// Failure handling tests:
// - when socket server is down, Send() should throw exception.
// - when socket server is up, continue Send() should succeed.
static void
TestSendRetry()
{
    // use std::promise to sync between server and client
    std::promise<void> masterReady;
    std::promise<void> threadReady;

    const std::string sockfile = TestUtil::GetCurrDir() + "/sockclient-retry";
    auto mockServer = std::make_shared<TestUtil::MockServer>(sockfile, false);

    auto serverTask = std::async(std::launch::async, [mockServer, &masterReady, &threadReady]() {
        threadReady.set_value();
        masterReady.get_future().wait();
        mockServer->Init();
        mockServer->Run();
        return mockServer->GetTotalBytesRead();
    });

    threadReady.get_future().wait();

    SocketClient client(sockfile, 10);

    const int nbytes = 30;
    const std::string testmsg = std::string(nbytes, 'A');
    size_t totalSend = nbytes + TestUtil::EndOfTest().size();

    // mockServer is not started yet. Send() should fail
    BOOST_CHECK_THROW(client.Send(testmsg.c_str()), SocketException);

    // start MockServer
    masterReady.set_value();

    for (int i = 0; i < 10; i++) {
        try {
            client.Send(testmsg.c_str());
        }
        catch(const SocketException& ex) {
            BOOST_TEST_MESSAGE("Send() failed with index=" << i);
            continue;
        }
        BOOST_TEST_MESSAGE("Send() succeeded with index=" << i);
        break;
    }

    client.Send(TestUtil::EndOfTest().c_str());

    bool mockServerDone = mockServer->WaitForTestsDone(100);
    BOOST_CHECK_EQUAL(true, mockServerDone);

    client.Stop();
    client.Close();
    mockServer->Stop();
    auto totalReceived = serverTask.get();

    BOOST_CHECK_EQUAL(totalSend, totalReceived);
}

BOOST_AUTO_TEST_CASE(Test_SocketClient_Send_Retry)
{
    try {
        TestSendRetry();
    }
    catch(const std::exception & ex) {
        BOOST_FAIL("Test failed with unexpected exception: " << ex.what());
    }
}

static void
DoSend(
    const std::shared_ptr<SocketClient>& sockClient,
    int senderIndex,
    int nmsgs,
    const std::string & testSource,
    const std::string & testData,
    const std::shared_ptr<CounterCV>& cv
    )
{
    CounterCVWrap cvwrap(cv);

    for (int i = 0; i < nmsgs; i++) {
        std::ostringstream strm;
        strm << testData << " " << senderIndex << "-" << i;
        DjsonLogItem item(testSource, strm.str());
        sockClient->Send(item.GetData());
    }
}

// Run Read() loop.
// Break the loop when either SocketClient is stopped, or
// Read() throws SocketException.
static void
DoRead(
    const std::shared_ptr<SocketClient>& sockClient,
    size_t & totalRead,
    const std::shared_ptr<CounterCV>& cv
    )
{
    CounterCVWrap cvwrap(cv);
    while(true) {
        try {
            char buf[1024];
            auto rtn = sockClient->Read(buf, sizeof(buf));
            if (-1 == rtn) {
                break;
            }
            totalRead += rtn;
        }
        catch(const SocketException& ex) {
            BOOST_TEST_MESSAGE("SocketClient::Read() exception: " << ex.what());
            break;
        }
    }
}

// Test multiple sender threads and multiple reader threads. Validate
// - no unexpected exception
// - data are received properly by the senders and readers.
static void
MultiSenderReaderTest(
    int nSenders,
    int nReaders,
    int nmsgs,
    size_t nbytesPerMsg
    )
{
    const std::string sockfile = TestUtil::GetCurrDir() + "/sockclient-mt";
    auto mockServer = std::make_shared<TestUtil::MockServer>(sockfile, true);
    mockServer->Init();

    auto serverTask = std::async(std::launch::async, [mockServer]() { mockServer->Run(); });

    auto sockClient = std::make_shared<SocketClient>(sockfile);

    const std::string testSource = "testSource";
    std::string testData = std::string(nbytesPerMsg, 'B');

    auto sendersCV = std::make_shared<CounterCV>(nSenders);
    auto readersCV = std::make_shared<CounterCV>(nReaders);

    std::vector<std::future<void>> senderList;
    std::vector<std::future<void>> readerList;

    for (int n = 0; n < nSenders; n++) {
        auto senderT = std::async(std::launch::async, DoSend, sockClient, n,
            nmsgs, testSource, testData, sendersCV);
        senderList.push_back(std::move(senderT));
    }

    size_t totalClientRead = 0;
    for (int i = 0; i < nReaders; i++) {
        auto readerT = std::async(std::launch::async, DoRead, sockClient,
            std::ref(totalClientRead), readersCV);
        readerList.push_back(std::move(readerT));
    }

    // wait until all senders are finished
    BOOST_CHECK_EQUAL(true, sendersCV->wait_for(500));

    // send end of test and shutdown both client and server
    sockClient->Send(TestUtil::EndOfTest().c_str());
    bool mockServerDone = mockServer->WaitForTestsDone(500);
    BOOST_CHECK_EQUAL(true, mockServerDone);

    sockClient->Stop();
    mockServer->Stop();

    // wait for mock server task to finish
    BOOST_CHECK_EQUAL(true, TestUtil::WaitForTask(serverTask, 100));

    // wait for reader tasks to finish
    BOOST_CHECK_EQUAL(true, readersCV->wait_for(100));

    sockClient->Close();

    // validate results

    // mininum data sent (actual data has extra info added to form valid DJSON msg)
    auto minClientSend = (testSource.size() + testData.size()) * nSenders * nmsgs;

    // Each ack msg sent from server to client should have at least 4 bytes
    auto minServerSend = 4 * nSenders * nmsgs;

    auto actualServerRecv = mockServer->GetTotalBytesRead();
    BOOST_TEST_MESSAGE("client sent(B) > " << minClientSend << "; server recv(B): "
        << actualServerRecv << "; client read(B): " << totalClientRead);

    BOOST_CHECK_GT(actualServerRecv, minClientSend);
    BOOST_CHECK_GE(totalClientRead, minServerSend);
}

BOOST_AUTO_TEST_CASE(Test_SocketClient_1_Sender_Reader_1_msg)
{
    try {
        MultiSenderReaderTest(1, 1, 1, 10);
    }
    catch(const std::exception & ex) {
        BOOST_FAIL("Test failed with unexpected exception: " << ex.what());
    }
}

BOOST_AUTO_TEST_CASE(Test_SocketClient_1_Sender_Reader_10_msg)
{
    try {
        MultiSenderReaderTest(1, 1, 10, 10);
    }
    catch(const std::exception & ex) {
        BOOST_FAIL("Test failed with unexpected exception: " << ex.what());
    }
}

BOOST_AUTO_TEST_CASE(Test_SocketClient_N_Sender_Reader)
{
    try {
        MultiSenderReaderTest(6, 3, 1000, 1000);
    }
    catch(const std::exception & ex) {
        BOOST_FAIL("Test failed with unexpected exception: " << ex.what());
    }
}

BOOST_AUTO_TEST_SUITE_END()
