#include <boost/test/unit_test.hpp>
#include <future>

#include "MockServer.h"
#include "SocketClient.h"
#include "Exceptions.h"
#include "DataReader.h"
#include "testutil.h"
#include "LogItemPtr.h"
#include "ConcurrentMap.h"

using namespace EndpointLog;

BOOST_AUTO_TEST_SUITE(testreader)


void
StartSocketReader(
    std::promise<void>& threadReady,
    const std::shared_ptr<DataReader>& sr,
    uint32_t minRunTimeMS
    )
{
    threadReady.set_value();

    auto startTime = std::chrono::system_clock::now();
    sr->Run();
    auto endTime = std::chrono::system_clock::now();
    auto runTimeMS = static_cast<uint32_t>((endTime-startTime)/std::chrono::milliseconds(1));
    BOOST_CHECK_GE(runTimeMS, minRunTimeMS);
}

// This test will do
// - start socket server
// - start a socket client to send data to the server.
// - start a socket reader to listen for data from server to client.
// - the server will read data from client and send response back.
// - validate reader can read the response data.
static void
SendDataToServer(int nmsgs)
{
    try {
        const std::string sockfile = TestUtil::GetCurrDir() + "/sockreader-bvt";
        auto mockServer = std::make_shared<TestUtil::MockServer>(sockfile);
        mockServer->Init();

        // start server in a thread
        auto serverTask = std::async(std::launch::async, [mockServer]() {
            mockServer->Run();
            return mockServer->GetTotalBytesRead();
        });

        auto sockClient = std::make_shared<SocketClient>(sockfile, 1);
        auto dataCache = std::make_shared<ConcurrentMap<LogItemPtr>>();

        // start reader in a thread
        auto sockReader = std::make_shared<DataReader>(sockClient, dataCache);
        auto readerTask = std::async(std::launch::async, [sockReader]() { sockReader->Run(); });

        size_t totalSend = 0;

        for (int i = 0; i < nmsgs; i++) {
            auto testData = "SocketClient test data " + std::to_string(i+1);
            sockClient->Send(testData.c_str());
            totalSend += testData.size();
        }
        // end of test
        sockClient->Send(TestUtil::EndOfTest().c_str());
        totalSend += TestUtil::EndOfTest().size();

        bool mockServerDone = mockServer->WaitForTestsDone(500);
        BOOST_CHECK_EQUAL(true, mockServerDone);

        sockClient->Stop();
        sockClient->Close();
        sockReader->Stop();
        readerTask.get();

        mockServer->Stop();

        auto nTagsRead = sockReader->GetNumTagsRead();
        auto nTagsWrite = mockServer->GetTotalTags();
        BOOST_CHECK_EQUAL(nTagsWrite, nTagsRead);

        auto totalReceived = serverTask.get();
        BOOST_CHECK_EQUAL(totalSend, totalReceived);
    }
    catch(const std::exception & ex) {
        BOOST_FAIL("Test exception " << ex.what());
    }    
}


BOOST_AUTO_TEST_CASE(Test_SocketReader_Error)
{
    try {
        const std::string socketfile = "/tmp/nosuchfile";
        auto sockClient = std::make_shared<SocketClient>(socketfile, 1);
        auto dataCache = std::make_shared<ConcurrentMap<LogItemPtr>>();

        const uint32_t minRunTimeMS = 100;
        std::promise<void> threadReady;

        auto sockReader = std::make_shared<DataReader>(sockClient, dataCache);
        auto readerTask = std::async(std::launch::async, StartSocketReader,
            std::ref(threadReady),sockReader, minRunTimeMS);

        // wait until StartSocketReader thread starts
        threadReady.get_future().wait();

        usleep(minRunTimeMS*1000);

        sockClient->Stop();
        sockReader->Stop();
        readerTask.get();
    }
    catch(const std::exception & ex) {
        BOOST_FAIL("Test exception " << ex.what());
    }
}

BOOST_AUTO_TEST_CASE(Test_SocketReader_Msg_1)
{
    SendDataToServer(1);
}

BOOST_AUTO_TEST_CASE(Test_SocketReader_Msg_100)
{
    SendDataToServer(100);
}

BOOST_AUTO_TEST_SUITE_END()
