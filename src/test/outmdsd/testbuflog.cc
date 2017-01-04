#include <boost/test/unit_test.hpp>
#include "BufferedLogger.h"
#include "DjsonLogItem.h"
#include "testutil.h"
#include "MockServer.h"

using namespace EndpointLog;

BOOST_AUTO_TEST_SUITE(testbuflog)

BOOST_AUTO_TEST_CASE(Test_BufferedLogger_Cstor_cache)
{
    try {
        BufferedLogger b("/tmp/nosuchfile", 1, 1, 1, 1);
    }
    catch(const std::exception & ex) {
        BOOST_FAIL("Test exception " << ex.what());
    }
}

BOOST_AUTO_TEST_CASE(Test_BufferedLogger_Cstor_nocache)
{
    try {
        BufferedLogger b("/tmp/nosuchfile", 0, 1, 1, 1);
    }
    catch(const std::exception & ex) {
        BOOST_FAIL("Test exception " << ex.what());
    }
}

BOOST_AUTO_TEST_CASE(Test_BufferedLogger_AddData_Null)
{
    try {
        BufferedLogger b("/tmp/nosuchfile", 1, 1, 1, 1);
        BOOST_CHECK_THROW(b.AddData(nullptr), std::invalid_argument);
    }
    catch(const std::exception & ex) {
        BOOST_FAIL("Test exception " << ex.what());
    }
}

// Validate BufferedLogger when socket server is down
static void
TestWhenSockServerIsDown(
    size_t nitems
    )
{
    int connRetryTimeoutMS = 1;
    BufferedLogger b("/tmp/nosuchfile", 100000, 100, connRetryTimeoutMS, nitems*10);

    for (size_t i = 0; i < nitems; i++) {
        LogItemPtr item(new DjsonLogItem("testsource", "testvalue-" + std::to_string(i+1)));
        b.AddData(item);
    }

    // wait timeout depends on nitems and connection retry, plus some runtime
    BOOST_CHECK_EQUAL(true, b.WaitUntilAllSend((connRetryTimeoutMS+10)*nitems+100));

    BOOST_CHECK_EQUAL(0, b.GetNumTagsRead());
    BOOST_CHECK_EQUAL(0, b.GetTotalSendSuccess());
    BOOST_CHECK_EQUAL(0, b.GetTotalResend());

    BOOST_CHECK_EQUAL(nitems, b.GetTotalSend());
    BOOST_CHECK_EQUAL(nitems, b.GetNumItemsInCache());
}

BOOST_AUTO_TEST_CASE(Test_BufferedLogger_ServerFailure_1)
{
    try {
        TestWhenSockServerIsDown(1);
    }
    catch(const std::exception & ex) {
        BOOST_FAIL("Test exception " << ex.what());
    }
}

BOOST_AUTO_TEST_CASE(Test_BufferedLogger_ServerFailure_100)
{
    try {
        TestWhenSockServerIsDown(100);
    }
    catch(const std::exception & ex) {
        BOOST_FAIL("Test exception " << ex.what());
    }
}

// Wait until client has nothing in cache to resend.
// Return true if success, false if timeout.
bool
WaitForClientCacheEmpty(
    BufferedLogger& bLogger,
    int timeoutMS
    )
{
    for (int i = 0; i < timeoutMS; i++) {
        if (0 == bLogger.GetNumItemsInCache()) {
            return true;
        }
        usleep(1000);
    }
    return false;
}

// Validate that server receives total nitems.
// And each msg is formated like CreateMsg(i)
static void
ValidateServerResults(
    const std::unordered_set<std::string>& serverDataSet,
    size_t nitems
    )
{
    BOOST_CHECK_EQUAL(1, serverDataSet.count(TestUtil::EndOfTest()));

    BOOST_CHECK_EQUAL(nitems+1, serverDataSet.size()); // end of test is an extra
    if (nitems+1 == serverDataSet.size()) {
        for (size_t i = 0; i < nitems; i++) {
            auto item = TestUtil::CreateMsg(i);
            BOOST_CHECK_MESSAGE(1 == serverDataSet.count(item), "Not found '" << item << "'");
        }
    }
}

static void
RunE2ETest(size_t nitems)
{
    const std::string sockfile = TestUtil::GetCurrDir() + "/buflog-e2e";
    auto mockServer = std::make_shared<TestUtil::MockServer>(sockfile, true);
    mockServer->Init();

    auto serverTask = std::async(std::launch::async, [mockServer]() { mockServer->Run(); });

    BufferedLogger bLogger(sockfile, 1000000, 100, 100, nitems*2);

    size_t totalSend = 0;
    for (size_t i = 0; i < nitems; i++) {
        auto testdata = TestUtil::CreateMsg(i);
        LogItemPtr item(new DjsonLogItem("testsource", testdata));
        bLogger.AddData(item);
        totalSend += testdata.size();
    }

    LogItemPtr eot(new DjsonLogItem("testsource", TestUtil::EndOfTest()));
    bLogger.AddData(eot);
    totalSend += TestUtil::EndOfTest().size();

    BOOST_CHECK_EQUAL(true, bLogger.WaitUntilAllSend(1000));
    BOOST_CHECK_EQUAL(true, mockServer->WaitForTestsDone(1000));
    BOOST_CHECK_EQUAL(true, WaitForClientCacheEmpty(bLogger, 1000));

    mockServer->Stop();
    serverTask.get();

    ValidateServerResults(mockServer->GetUniqDataRead(), nitems);

    BOOST_CHECK_LT(0, mockServer->GetTotalTags());

    BOOST_CHECK_GT(mockServer->GetTotalBytesRead(), totalSend);

    BOOST_CHECK_LE(nitems+1, bLogger.GetNumTagsRead());
    BOOST_CHECK_EQUAL(nitems+1, bLogger.GetTotalSendSuccess());

    BOOST_CHECK_LE(nitems+1, bLogger.GetTotalSend());
    BOOST_CHECK_EQUAL(0, bLogger.GetNumItemsInCache());
}

BOOST_AUTO_TEST_CASE(Test_BufferedLogger_E2E_1)
{
    try {
        RunE2ETest(1);
    }
    catch(const std::exception & ex) {
        BOOST_FAIL("Test exception " << ex.what());
    }
}

BOOST_AUTO_TEST_CASE(Test_BufferedLogger_E2E_1000)
{
    try {
        RunE2ETest(1000);
    }
    catch(const std::exception & ex) {
        BOOST_FAIL("Test exception " << ex.what());
    }
}

BOOST_AUTO_TEST_SUITE_END()
