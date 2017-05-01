#include <boost/test/unit_test.hpp>
#include <vector>

#include "MockServer.h"
#include "SocketLogger.h"
#include "SocketClient.h"
#include "DataReader.h"
#include "testutil.h"

using namespace EndpointLog;

BOOST_AUTO_TEST_SUITE(testeplog)

// Test that SocketLogger constructor and destructor
// work properly
BOOST_AUTO_TEST_CASE(Test_SocketLogger_Constructor)
{
    try {
        SocketLogger eplog("/tmp/unknownfile", 100, 1000);
    }
    catch(const std::exception & ex) {
        BOOST_FAIL("Test exception " << ex.what());
    }
}

BOOST_AUTO_TEST_CASE(Test_SocketLogger_Error)
{
    try {
        SocketLogger eplog("/tmp/unknownfile", 100, 1000, 1);

        for (int i = 0; i < 5; i++) {
            bool sendOK = eplog.SendDjson("testSource", "testSchemaAndData-" + std::to_string(i+1));
            BOOST_CHECK(!sendOK);
        }
    }
    catch(const std::exception & ex) {
        BOOST_FAIL("Test exception " << ex.what());
    }
}

// Return number of bytes sent.
// If SendDjson() fails, return 0.
// Save the index of each failed send to 'failedMsgList' for future resend.
static size_t
SendOnce(
    SocketLogger& eplog,
    int msgIndex,
    int sendDelayMS,
    std::vector<int>& failedMsgList
    )
{
    auto data = TestUtil::CreateMsg(msgIndex);

    if (!eplog.SendDjson("testSource", data)) {
        failedMsgList.push_back(msgIndex);
        return 0;
    }

    usleep(sendDelayMS * 1000); // add some delay for test purpose
    return data.size();
}

static size_t
SendEndOfTestToServer(
    SocketLogger& eplog
    )
{
    bool sendOK = eplog.SendDjson("testSource", TestUtil::EndOfTest());
    if (!sendOK) {
        return 0;
    }
    return TestUtil::EndOfTest().size();
}

// Wait until client has nothing in cache to resend.
// Return true if success, false if timeout.
bool
WaitForClientCacheEmpty(
    SocketLogger& eplog,
    int timeoutMS
    )
{
    for (int i = 0; i < timeoutMS; i++) {
        if (0 == eplog.GetNumItemsInCache()) {
            return true;
        }
        usleep(1000);
    }
    return false;
}

// Validate that server receives total nmsgs.
// And each msg is formated like CreateMsg(i)
static void
ValidateServerResults(
    const std::unordered_set<std::string>& serverDataSet,
    size_t nmsgs
    )
{
    BOOST_CHECK_EQUAL(1, serverDataSet.count(TestUtil::EndOfTest()));

    BOOST_CHECK_EQUAL(nmsgs+1, serverDataSet.size()); // end of test is an extra
    if (nmsgs+1 == serverDataSet.size()) {
        for (size_t i = 0; i < nmsgs; i++) {
            auto item = TestUtil::CreateMsg(i);
            BOOST_CHECK_MESSAGE(1 == serverDataSet.count(item), "Not found '" << item << "'");
        }
    }
}

// This is to test that the logger can send and receive data reliably.
// If the socket server has failures, data should be resent properly.
//
// sendDelayMS: milliseconds to delay between each Send().
// mockServerRestart: if true, restart MockServer in the middle to simulate mdsd failures.
static size_t
SendDataToServer(
    const std::shared_ptr<TestUtil::MockServer>& mockServer,
    const std::string & sockfile,
    int nmsgs,
    int sendDelayMS,
    bool mockServerRestart
    )
{
    BOOST_TEST_MESSAGE("Start to SendDataToServer: nmsgs=" << nmsgs << "; RestartServer=" << mockServerRestart);

    size_t totalSend = 0;
    int testRuntimeMS = 400;

    // Make ackTimeout long so that it never timeout.
    // Make resentTimeout short so that it will resend when failed
    SocketLogger eplog(sockfile, testRuntimeMS*10, testRuntimeMS/4, testRuntimeMS/4);

    auto batchSize = nmsgs / 3;
    std::vector<int> failedMsgList;

    for (int i = 0; i < batchSize; i++) {
        totalSend += SendOnce(eplog, i, sendDelayMS, failedMsgList);
    }

    std::future<void> serverTask;

    if (mockServerRestart) {
        std::promise<void> thReady;

        auto disconnectMS = sendDelayMS * batchSize;
        serverTask = std::async(std::launch::async, [mockServer, &thReady, disconnectMS] () {
            thReady.set_value();
            mockServer->DisconnectAndRun(disconnectMS);
        });

        thReady.get_future().wait(); // wait until mockServer starts to call DisconnectAndRun()
    }

    for (int i = batchSize; i < nmsgs; i++) {
        totalSend += SendOnce(eplog, i, sendDelayMS, failedMsgList);
    }

    // resend failed cases
    std::vector<int> resendFailList;
    for (const auto & i : failedMsgList) {
        totalSend += SendOnce(eplog, i, 0, resendFailList);
    }
    BOOST_CHECK_EQUAL(0, resendFailList.size());

    WaitForClientCacheEmpty(eplog, testRuntimeMS);

    totalSend += SendEndOfTestToServer(eplog);

    bool mockServerDone = mockServer->WaitForTestsDone(testRuntimeMS);
    BOOST_CHECK(mockServerDone);

    mockServer->Stop();

    if (serverTask.valid()) {
        serverTask.get();
    }

    ValidateServerResults(mockServer->GetUniqDataRead(), nmsgs);

    return totalSend;
}

// Send messages to MockServer from SocketLogger.
// sendDelayMS: milliseconds to delay between each Send().
// mockServerRestart: if true, restart MockServer in the middle to simulate mdsd failures.
// validate: E2E works
static void
TestClientServerE2E(
    int nmsgs,
    int sendDelayMS,
    bool mockServerRestart
    )
{
    try {
        const std::string sockfile = TestUtil::GetCurrDir() + "/eplog-bvt";
        auto mockServer = std::make_shared<TestUtil::MockServer>(sockfile);
        mockServer->Init();

        // start server in a thread
        auto serverTask = std::async(std::launch::async, [mockServer]() {
            mockServer->Run();
        });

        auto totalSend = SendDataToServer(mockServer, sockfile, nmsgs, sendDelayMS, mockServerRestart);

        // wait for mockServer to finish
        serverTask.get();

        auto totalReceived = mockServer->GetTotalBytesRead();

        // because EndpoingLogger will add msg id, schema id etc, the totalSend and totalReceived
        // won't be equal
        BOOST_TEST_MESSAGE("TotalSend=" << totalSend << "; TotalReceived=" << totalReceived);
        BOOST_CHECK_LT(totalSend, totalReceived);
    }
    catch(const std::exception & ex) {
        BOOST_FAIL("Test exception: " << ex.what());
    }    
}

BOOST_AUTO_TEST_CASE(Test_SocketLogger_Msg_1)
{
    TestClientServerE2E(1, 0, false);
}

BOOST_AUTO_TEST_CASE(Test_SocketLogger_Msg_100)
{
    TestClientServerE2E(100, 0, false);
}

BOOST_AUTO_TEST_CASE(Test_SocketLogger_Failure_1)
{
    TestClientServerE2E(1, 10, true);
}

BOOST_AUTO_TEST_CASE(Test_SocketLogger_Failure_12)
{
    TestClientServerE2E(12, 10, true);
}

BOOST_AUTO_TEST_CASE(Test_SocketLogger_Failure_1000)
{
    TestClientServerE2E(1000, 1, true);
}

BOOST_AUTO_TEST_SUITE_END()

