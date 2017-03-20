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

            BOOST_CHECK_MESSAGE((runTimeMS >= timeoutMS),
                "Iteration " << i << " timed out under " << timeoutMS << " (" << runTimeMS << ") ms");
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
        BOOST_CHECK(mockServerDone);

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
            continue;
        }
        break;
    }

    client.Send(TestUtil::EndOfTest().c_str());

    bool mockServerDone = mockServer->WaitForTestsDone(100);
    BOOST_CHECK(mockServerDone);

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
    const std::shared_ptr<CounterCV>& cv,
    std::promise<void>& thReady
    )
{
    CounterCVWrap cvwrap(cv);
    thReady.set_value();

    for (int i = 0; i < nmsgs; i++) {
        std::ostringstream strm;
        strm << testData << " " << senderIndex << "-" << i;
        DjsonLogItem item(testSource, strm.str());
        sockClient->Send(item.GetData());
    }
}

// Run Read() loop.
// When min number of bytes is read, signal minReadCV.
// Break the loop when either SocketClient is stopped, or
// Read() throws SocketException.
static void
DoRead(
    const std::shared_ptr<SocketClient>& sockClient,
    size_t minReadBytes,
    const std::shared_ptr<CounterCV>& cv,
    const std::shared_ptr<CounterCV>& minReadCV,
    std::promise<void>& thReady
    )
{
    CounterCVWrap cvwrap(cv);
    thReady.set_value();
    size_t totalRead = 0;

    while(true) {
        try {
            char buf[1024];
            auto rtn = sockClient->Read(buf, sizeof(buf));
            if (-1 == rtn) {
                BOOST_TEST_MESSAGE("SocketClient::Read() returned -1. Abort CV=" << cvwrap.GetId());
                break;
            }
            totalRead += rtn;
            if (totalRead >= minReadBytes) {
                minReadCV->notify_all();
            }
        }
        catch(const SocketException& ex) {
            BOOST_TEST_MESSAGE("SocketClient::Read() exception: " << ex.what() << ". Abort CV=" << cvwrap.GetId());
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

    // Make sure each reader reads at least N bytes before signaling CV
    auto minReadCV = std::make_shared<CounterCV>(nReaders);

    // use std::promise to make sure all threads are ready before
    // master thread moves ahead
    std::vector<std::promise<void>> thReadyList;
    thReadyList.reserve(nSenders+nReaders);
    for (int i = 0; i < nSenders+nReaders; i++) {
        thReadyList.push_back(std::promise<void>());
    }

    std::vector<std::future<void>> senderList;
    std::vector<std::future<void>> readerList;

    for (int n = 0; n < nSenders; n++) {
        auto senderT = std::async(std::launch::async, DoSend, sockClient, n,
            nmsgs, testSource, testData, sendersCV, std::ref(thReadyList[n]));
        senderList.push_back(std::move(senderT));
    }

    // Each ack msg sent from server to client should have at least 4 bytes
    auto minReadBytes = 4 * nmsgs;
    for (int i = 0; i < nReaders; i++) {
        auto readerT = std::async(std::launch::async, DoRead, sockClient,
            minReadBytes, readersCV, minReadCV, std::ref(thReadyList[i+nSenders]));
        readerList.push_back(std::move(readerT));
    }

    // wait for all threads to be ready
    for (auto & t : thReadyList) {
        t.get_future().wait();
    }

    // wait until all senders are finished
    // NOTE: the wait timeout should depend on nmsgs, and machine hardware.
    // Allocate enough time so tests can run on slow machines.
    auto timeoutMS = 500 + nmsgs * 5;
    BOOST_CHECK_MESSAGE(sendersCV->wait_for(timeoutMS),
        "Wait for sendersCV timed out (" << timeoutMS << " ms)");

    // send end of test and shutdown both client and server
    sockClient->Send(TestUtil::EndOfTest().c_str());
    auto msTimeoutMS = 500;
    BOOST_CHECK_MESSAGE(mockServer->WaitForTestsDone(msTimeoutMS),
        "Wait for mockServer tests to finish timed out (" << msTimeoutMS << " ms)");

    // wait for each reader to read at least minReadBytes bytes before calling Stop().
    BOOST_CHECK_MESSAGE(minReadCV->wait_for(msTimeoutMS),
        "Wait for minReadBytes timed out (" << msTimeoutMS << " ms)");

    sockClient->Stop();
    mockServer->Stop();

    // wait for mock server task to finish
    auto serverTaskTimeout = 100;
    BOOST_CHECK_MESSAGE(TestUtil::WaitForTask(serverTask, serverTaskTimeout),
        "Wait for serverTask timed out (" << serverTaskTimeout << " ms)");

    // wait for reader tasks to finish
    auto readersCVTimeout = 100;
    BOOST_CHECK_MESSAGE(readersCV->wait_for(readersCVTimeout),
        "Wait for readersCV timed out (" << readersCVTimeout << " ms)");

    sockClient->Close();

    // validate results

    // minimum data sent (actual data has extra info added to form valid DJSON msg)
    auto minClientSend = (testSource.size() + testData.size()) * nSenders * nmsgs;
    auto actualServerRecv = mockServer->GetTotalBytesRead();
    BOOST_CHECK_GT(actualServerRecv, minClientSend);
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
