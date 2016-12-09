#include <boost/test/unit_test.hpp>
#include <future>

#include "SocketClient.h"
#include "Exceptions.h"
#include "DataReader.h"
#include "DataResender.h"
#include "ConcurrentMap.h"
#include "testutil.h"
#include "MockServer.h"

using namespace EndpointLog;

BOOST_AUTO_TEST_SUITE(testsocket)

BOOST_AUTO_TEST_CASE(Test_SocketClient_Exception)
{
    try {
        const std::string socketfile = "/tmp/nosuchfile";
        SocketClient client(socketfile, 1);

        client.Connect();

        const char* testData = "SocketClient test data";
        BOOST_CHECK_THROW(client.Send(testData), SocketException);

        char buf[128];
        BOOST_CHECK_THROW(client.Read(buf, sizeof(buf), 1), SocketException);
    }
    catch(const std::exception & ex) {
        BOOST_FAIL("Test exception " << ex.what());
    }
}

BOOST_AUTO_TEST_CASE(Test_SocketClient_Reconnect_Error)
{
    try {
        const std::string socketfile = "/tmp/nosuchfile";
        SocketClient client(socketfile, 1);

        const char* testData = "SocketClient test data";
        int ntimes = 4;
        for (int i = 0; i < ntimes; i++) {
            BOOST_CHECK_THROW(client.Send(testData), SocketException);
        }
        BOOST_CHECK_EQUAL(ntimes, client.GetNumReConnect());
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

BOOST_AUTO_TEST_SUITE_END()
